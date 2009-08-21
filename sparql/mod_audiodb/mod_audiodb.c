

#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "http_protocol.h"
#include <ap_config.h>
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

module AP_MODULE_DECLARE_DATA audiodb_module;

typedef struct {
	char *dbpath;
} adb_config;

/**
 * Config bits and pieces
 **/

static void *create_audiodb_config(apr_pool_t* p, server_rec* s) {
	adb_config *config = (adb_config *)apr_pcalloc(p, sizeof(adb_config));
	config->dbpath = NULL;
	return config;
}

static const char* set_database_path(cmd_parms *parms, void *mconfig, const char *arg) {
	adb_config *config = ap_get_module_config(parms->server->module_config, &audiodb_module);
	config->dbpath = (char *)arg;
	return NULL;
}

static const command_rec mod_audiodb_cmds[] = {

	AP_INIT_TAKE1("DatabasePath", set_database_path, NULL, RSRC_CONF, "The AudioDB database to use"),
	{NULL}
};

static int log_message(void *user_data, librdf_log_message *message) {
	fprintf(stderr, "%s\n", librdf_log_message_message(message));
	fflush(stderr);
	return 1;
}


static int adb_handle_sparql_req(request_rec *r) {
	librdf_world* world = NULL;
	librdf_storage* storage = NULL;
	librdf_uri *output_uri = NULL;
	int rc = DECLINED;

	if(strcmp(r->handler, "audiodb-sparql-handler") != 0) {
		goto error;
	}

	rc = OK;

	adb_config* config = ap_get_module_config(r->server->module_config,
		&audiodb_module);

	r->status = HTTP_BAD_REQUEST;

	if(!r->args) {
		r->args = "";
	}

	const apr_table_t *form_table;
	apreq_handle_t *h = apreq_handle_apache2(r);


	if (r->method_number == M_POST) {
		if (apreq_body(h, &form_table) != APR_SUCCESS)
			goto error;
	}
	else {
		if (apreq_args(h, &form_table) != APR_SUCCESS) 
			goto error;
	}


	const char *query_string = apr_table_get(form_table, "query");
	const char *output = apr_table_get(form_table, "output");

	if(!query_string)
		query_string = "DESCRIBE ";

	world = librdf_new_world();
	librdf_world_open(world);
	librdf_world_set_logger(world, NULL, log_message);

	if(!config->dbpath) {
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, r, "DatabasePath is required");
		goto error;
	}	
	
	// First make sure we actually have a valid query
	librdf_query *query;
	if (!(query = librdf_new_query(world, "sparql", NULL, (unsigned char*)query_string, NULL))) {
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, r, "Unable to parse query");
		ap_rprintf(r, "Unable to parse query"); 
		goto error;
	}

	storage = librdf_new_storage(world, "audiodb", config->dbpath, NULL);
	if(!storage) {
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, r, "Unable to open audioDB: %s", config->dbpath);
		ap_rprintf(r, "Unable to open audioDB"); 
		goto error;
	}

	librdf_model *model;
	if (!(model = librdf_new_model(world, storage, NULL))) {
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, r, "Unable to create model");
		ap_rprintf(r, "Unable to create model"); 
		goto error;
	}

	librdf_query_results *results;
	if (!(results = librdf_query_execute(query, model))) {
		ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, 0, r, "Unable to execute query");
		ap_rprintf(r, "Unable to execute query"); 
		goto error;
	}

	

	if(!output)
		output = "application/sparql-results+xml";

	if(strcmp(output, "text/json") == 0) {
		r->content_type = "text/json";
		output_uri = librdf_new_uri( world,(unsigned char *) JSON_URI );
	}
	else {
		r->content_type = "application/sparql-results+xml";
		output_uri = librdf_new_uri( world,(unsigned char *) SPARQL_URI );
	}
	const unsigned char* out = librdf_query_results_to_string(results, output_uri, librdf_new_uri(world, (unsigned char*) BASE_URI)); 
	
	ap_rprintf(r, out); 
	
	r->status = HTTP_OK;

	error:

	if(output_uri)
		librdf_free_uri(output_uri);

	if(storage) {
		librdf_storage_close(storage);
		librdf_free_storage(storage);
	}

	if(world)
		librdf_free_world(world);

	return rc; 

}

static void mod_audiodb_register_hooks (apr_pool_t *p) {
	ap_hook_handler(adb_handle_sparql_req, NULL, NULL, APR_HOOK_FIRST);
}

module AP_MODULE_DECLARE_DATA audiodb_module = {
	STANDARD20_MODULE_STUFF,
	NULL,
	NULL,
	create_audiodb_config,
	NULL,
	mod_audiodb_cmds,
	mod_audiodb_register_hooks,
};
