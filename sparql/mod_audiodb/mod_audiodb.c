
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

static int ap_dump_table(void *baton, const char *key, const char *value)
{
    if (key && value)
        fprintf(stderr, "%s:%s\n", key, value);
    fflush(stderr);
    return 1;
}

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

	const unsigned char *query_string = apr_table_get(form_table, "query");

	int rc = 0;

	librdf_world* world = librdf_new_world();
	if(!world)
	{
		rc = 1;
		goto error;
	}


	librdf_world_open(world);
	librdf_world_set_logger(world, NULL, log_out);
	librdf_storage* storage = librdf_new_storage(world, "audiodb", "/tmp/test.adb", "new='yes'");
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
	if (!(query = librdf_new_query(world, "sparql", NULL, query_string, NULL)))
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

	ap_rprintf(r, "Everything went awesomely!");

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
