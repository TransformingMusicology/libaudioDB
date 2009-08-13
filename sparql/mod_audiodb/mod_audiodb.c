
#include <httpd.h>
#include <ap_config.h>
#include <http_config.h>
#include <http_protocol.h>
#include <apr_strings.h>
#include <librdf.h>
#include <apreq_module.h>
#include <apreq_parser.h>
#include <apreq_param.h>
#include "apreq2/apreq_module_apache2.h"
#include <rasqal.h>

#define BASE_URI "http://purl.org/ontology/af/"
#define JSON_URI "http://www.w3.org/2001/sw/DataAccess/json-sparql/"
#define SPARQL_URI "http://www.w3.org/TR/2006/WD-rdf-sparql-XMLres-20070614/"

static int log_out(void *user_data, librdf_log_message *message)
{
	fprintf(stderr, "%s\n", librdf_log_message_message(message));
	fflush(stderr);
	return 1;
}

static int adb_handle_sparql_req(request_rec *r) {
	if(strcmp(r->handler, "audiodb-sparql-handler") != 0) {
		return DECLINED;
	}

	r->content_type = "text/plain";
	r->status = OK;
	r->status_line = "200 OK";

	if(!r->args) {
		r->args = "";
	}

	const apr_table_t *form_table;
	apreq_handle_t *h = apreq_handle_apache2(r);
	if(apreq_args(h, &form_table) != APR_SUCCESS)
		return DECLINED;

	const char *query_string = apr_table_get(form_table, "query");
	const char *output = apr_table_get(form_table, "output");
	if(!output)
		output = "xml";

	int rc = 0;
	librdf_world* world = librdf_new_world();
	librdf_world_open(world);
	librdf_world_set_logger(world, NULL, log_out);
	librdf_storage* storage = librdf_new_storage(world, "audiodb", "/tmp/test_database.adb", NULL);
	if(!storage)
	{
		rc = 2;
		goto error;
	}

	librdf_model *model;
	if (!(model = librdf_new_model(world, storage, NULL)))
	{
		rc = 5;	
		goto error;
	}

	librdf_query *query;
	if (!(query = librdf_new_query(world, "sparql", NULL, (unsigned char*)query_string, NULL)))
	{
		rc = 3;
		goto error;
	}

	librdf_query_results *results;
	if (!(results = librdf_query_execute(query, model)))
	{
		rc = 4;
		goto error;
	}


	librdf_uri *output_uri;

	if(strcmp(output, "json") == 0)
		output_uri = librdf_new_uri( world,(unsigned char *) JSON_URI );
	else
		output_uri = librdf_new_uri( world,(unsigned char *) SPARQL_URI );

	const unsigned char* out = librdf_query_results_to_string(results, output_uri, librdf_new_uri(world, (unsigned char*) BASE_URI)); 
	
	librdf_free_uri(output_uri);
	librdf_storage_close(storage);
	librdf_free_storage(storage);
	librdf_free_world(world);
	ap_rprintf(r, out); 

	rc = 0;
	return r->status;

	error:
		ap_rprintf(r, "Fail %d", rc);
		return OK;
}

static void mod_audiodb_register_hooks (apr_pool_t *p) {
	ap_hook_handler(adb_handle_sparql_req, NULL, NULL, APR_HOOK_FIRST);
}

module AP_MODULE_DECLARE_DATA audiodb_module = {
	STANDARD20_MODULE_STUFF,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	mod_audiodb_register_hooks,
};
