#ifdef HAVE_CONFIG_H
#include <rdf_config.h>
#endif

#ifdef WIN32
#include <win32_rdf_config.h>
#endif

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h> /* for abort() as used in errors */
#endif
#include <sys/types.h>

#include <redland.h>
#include <audioDB_API.h>


typedef struct {
	librdf_model* model;
	librdf_storage* storage;
	char *name;
	size_t name_len;
	int is_new;

	adb_t *adb;

} librdf_storage_audiodb_instance;

typedef struct {
	librdf_storage* storage;
	librdf_storage_audiodb_instance* audiodb_context;
	int finished;
	librdf_statement* statement;
	librdf_statement* query_statement;
	librdf_node* context;

} librdf_storage_audiodb_find_statements_stream_context;

static int librdf_storage_audiodb_init(librdf_storage* storage, const char *name, librdf_hash* options);
static int librdf_storage_audiodb_open(librdf_storage* storage, librdf_model* model);
static int librdf_storage_audiodb_close(librdf_storage* storage);
static int librdf_storage_audiodb_size(librdf_storage* storage);
static int librdf_storage_audiodb_add_statement(librdf_storage* storage, librdf_statement* statement);
static int librdf_storage_audiodb_add_statements(librdf_storage* storage, librdf_stream* statement_stream);
static int librdf_storage_audiodb_remove_statement(librdf_storage* storage, librdf_statement* statement);
static int librdf_storage_audiodb_contains_statement(librdf_storage* storage, librdf_statement* statement);
static librdf_stream* librdf_storage_audiodb_serialise(librdf_storage* storage);
static librdf_stream* librdf_storage_audiodb_find_statements(librdf_storage* storage, librdf_statement* statement);

/* find_statements implementing functions */
static int librdf_storage_audiodb_find_statements_end_of_stream(void* context);
static int librdf_storage_audiodb_find_statements_next_statement(void* context);
static void* librdf_storage_audiodb_find_statements_get_statement(void* context, int flags);
static void librdf_storage_audiodb_find_statements_finished(void* context);

static int librdf_storage_audiodb_sync(librdf_storage *storage);

static void librdf_storage_audiodb_register_factory(librdf_storage_factory *factory);
#ifdef MODULAR_LIBRDF
void librdf_storage_module_register_factory(librdf_world *world);
#endif


static int librdf_storage_audiodb_init(librdf_storage* storage, const char *name, librdf_hash* options) {
	
	librdf_storage_audiodb_instance* context;
	char* name_copy;

	context = (librdf_storage_audiodb_instance*)LIBRDF_CALLOC(librdf_storage_audiodb_instance, 1, sizeof(librdf_storage_audiodb_instance));

	if(!context)
	{
		if(options)
			librdf_free_hash(options);
		return 1;
	}

	librdf_storage_set_instance(storage, context);

	context->storage = storage;
	
	// Store the name of the db
	context->name_len=strlen(name);
	name_copy=(char*)LIBRDF_MALLOC(cstring, context->name_len+1);
	if(!name_copy) {
		if(options)
			librdf_free_hash(options);
		return 1;
	}
	strncpy(name_copy, name, context->name_len+1);
	context->name=name_copy;
		
	if(librdf_hash_get_as_boolean(options, "new") > 0)
		context->is_new = 1;

	if(options)
		librdf_free_hash(options);

	librdf_log(storage->world, 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
		"Initialised!");

	return 0;
}

static int librdf_storage_audiodb_open(librdf_storage* storage, librdf_model* model) {

	librdf_storage_audiodb_instance* context = (librdf_storage_audiodb_instance*)storage->instance;	
	int db_file_exists = 0;

	librdf_log(storage->world, 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
	"open");
	
	if(!access((const char*)context->name, F_OK))
		db_file_exists = 1;
	else
		context->is_new = 1;

	if(context->is_new && db_file_exists)
		unlink(context->name);

	context->adb = NULL;
	
	if(context->is_new) {
		if(!(context->adb = audiodb_create(context->name, 0, 0, 0))) {
			librdf_log(storage->world, 0, LIBRDF_LOG_ERROR, LIBRDF_FROM_STORAGE, NULL,
					"Unable to create %s", context->name);
			return 1;
		}
	}
	else
	{
		if(!(context->adb = audiodb_open(context->name, O_RDWR))) {	
			librdf_log(storage->world, 0, LIBRDF_LOG_ERROR, LIBRDF_FROM_STORAGE, NULL,
					"Unable to open %s", context->name);
			return 1;
		}
	}

	return 0;
}

static int librdf_storage_audiodb_close(librdf_storage* storage) {
	librdf_storage_audiodb_instance* context = (librdf_storage_audiodb_instance*)storage->instance;	
	
	librdf_log(storage->world, 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
	"close");

	if(context->adb)
	{
		audiodb_close(context->adb);
		context->adb = NULL;
	}

	return 0;
}

static int librdf_storage_audiodb_size(librdf_storage* storage) {
	librdf_log(storage->world, 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
		"size");
	return 0;
}

static int librdf_storage_audiodb_add_statement(librdf_storage* storage, librdf_statement* statement) {
	librdf_log(storage->world, 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
	"add statement");
	return 0;
}

static int librdf_storage_audiodb_add_statements(librdf_storage* storage, librdf_stream* statement_stream) {
	librdf_log(storage->world, 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
	"add statements");
	return 0;
}

static int librdf_storage_audiodb_remove_statement(librdf_storage* storage, librdf_statement* statement) {
	librdf_log(storage->world, 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
	"remove statement");
	return 0;
}

static int librdf_storage_audiodb_contains_statement(librdf_storage* storage, librdf_statement* statement) {
	librdf_log(storage->world, 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
		"Contains statement");
	return 0;
}

static librdf_stream* librdf_storage_audiodb_serialise(librdf_storage* storage) {
	librdf_log(storage->world, 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
	"serialise");
	return NULL;
}

static librdf_stream* librdf_storage_audiodb_find_statements(librdf_storage* storage, librdf_statement* statement) {

	librdf_storage_audiodb_instance* context = (librdf_storage_audiodb_instance*)storage->instance;	
	librdf_storage_audiodb_find_statements_stream_context* scontext;
	librdf_stream* stream;
	
	librdf_log(storage->world, 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
	"find statements %s", librdf_statement_to_string(statement));

	scontext = (librdf_storage_audiodb_find_statements_stream_context*)LIBRDF_CALLOC(librdf_storage_audiodb_find_statements_stream_context, 1, sizeof(librdf_storage_audiodb_find_statements_stream_context));

	if(!scontext)
		return NULL;

	scontext->storage = storage;
	librdf_storage_add_reference(scontext->storage);

	scontext->audiodb_context = context;

	scontext->query_statement = librdf_new_statement_from_statement(statement);
	if(!scontext->query_statement) {
		librdf_storage_audiodb_find_statements_finished((void*)scontext);
		return NULL;
	}

	stream = librdf_new_stream(storage->world,
								(void*)scontext,
								&librdf_storage_audiodb_find_statements_end_of_stream,
								&librdf_storage_audiodb_find_statements_next_statement,
								&librdf_storage_audiodb_find_statements_get_statement,
								&librdf_storage_audiodb_find_statements_finished);

	if(!stream) {
		librdf_storage_audiodb_find_statements_finished((void*)scontext);
		return NULL;
	}

	return stream;
}

static void librdf_storage_audiodb_find_statements_finished(void* context) {
	librdf_storage_audiodb_find_statements_stream_context* scontext=(librdf_storage_audiodb_find_statements_stream_context*)context;

	if(scontext->storage)
		librdf_storage_remove_reference(scontext->storage);

	if(scontext->query_statement)
		librdf_free_statement(scontext->query_statement);

	if(scontext->statement)
		librdf_free_statement(scontext->statement);

	if(scontext->context)
		librdf_free_node(scontext->context);

	LIBRDF_FREE(librdf_storage_audiodb_find_statements_stream_context, scontext);
}

static int librdf_storage_audiodb_get_next_common(librdf_storage_audiodb_instance* scontext,
													librdf_statement **statement,
													librdf_node **context_node) {

	librdf_node* node;


	if(!*statement) {
		if(!(*statement = librdf_new_statement(scontext->storage->world)))
			return 1;
	}
	
	librdf_log(scontext->storage->world, 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
			"Handle statement %s", librdf_statement_to_string(statement));

	librdf_statement_clear(*statement);

	node = librdf_new_node_from_uri_string(scontext->storage->world, "testing");
	
	if(!node)
		return 1;

	librdf_statement_set_subject(*statement, node);

	node = librdf_new_node_from_uri_string(scontext->storage->world, "foootle");
	
	if(!node)
		return 1;

	librdf_statement_set_predicate(*statement, node);

	node = librdf_new_node_from_uri_string(scontext->storage->world, "barble");
	
	if(!node)
		return 1;

	librdf_statement_set_object(*statement, node);

	return -1;
}

static int librdf_storage_audiodb_find_statements_end_of_stream(void* context) {
	librdf_storage_audiodb_find_statements_stream_context* scontext=(librdf_storage_audiodb_find_statements_stream_context*)context;

	if(scontext->finished)
		return 1;
	
	if(scontext->statement == NULL) {
		int result;
		result = librdf_storage_audiodb_get_next_common(scontext->audiodb_context,
														&scontext->statement,
														&scontext->context);

		librdf_log(scontext->storage->world, 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
			"Handle eos statement %s %d", librdf_statement_to_string(scontext->query_statement), result);

		if(result) {
			scontext->finished = 1;
		}
	}
	return scontext->finished;

}

static int librdf_storage_audiodb_find_statements_next_statement(void* context) {
	librdf_storage_audiodb_find_statements_stream_context* scontext=(librdf_storage_audiodb_find_statements_stream_context*)context;
	int result;
	

	if(scontext->finished)
		return 1;
		
	result = librdf_storage_audiodb_get_next_common(scontext->audiodb_context,
														&scontext->statement,
														&scontext->context);

	librdf_log(scontext->storage->world, 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
		"Handle next statement %s %d", librdf_statement_to_string(scontext->query_statement), result);

	if(result) {
		scontext->finished = 1;
	}

	return result;
}

static void* librdf_storage_audiodb_find_statements_get_statement(void* context, int flags) {
	librdf_storage_audiodb_find_statements_stream_context* scontext=(librdf_storage_audiodb_find_statements_stream_context*)context;
	
	switch(flags) {
		case LIBRDF_ITERATOR_GET_METHOD_GET_OBJECT:
			return scontext->statement;
		case LIBRDF_ITERATOR_GET_METHOD_GET_CONTEXT:
			return scontext->context;
		default:
			librdf_log(scontext->storage->world,
				0, LIBRDF_LOG_ERROR, LIBRDF_FROM_STORAGE, NULL,
 				"Unknown iterator method flag %d", flags);
			return NULL;
	}

}



static int librdf_storage_audiodb_sync(librdf_storage *storage) {
	return 0;
}

static void 
librdf_storage_audiodb_register_factory(librdf_storage_factory *factory) {
		LIBRDF_ASSERT_CONDITION(!strcmp(factory->name, "audiodb"));

	
		factory->version            = LIBRDF_STORAGE_INTERFACE_VERSION;
		factory->init               = librdf_storage_audiodb_init;
		factory->open               = librdf_storage_audiodb_open;
		factory->close              = librdf_storage_audiodb_close;
		factory->size               = librdf_storage_audiodb_size;
		factory->add_statement      = librdf_storage_audiodb_add_statement;
		factory->remove_statement   = librdf_storage_audiodb_remove_statement;
		factory->contains_statement = librdf_storage_audiodb_contains_statement;
		factory->serialise          = librdf_storage_audiodb_serialise;
		factory->find_statements    = librdf_storage_audiodb_find_statements;
}

#ifdef MODULAR_LIBRDF

/** Entry point for dynamically loaded storage module */
void librdf_storage_module_register_factory(librdf_world *world) {
		librdf_storage_register_factory(world, "audiodb", "AudioDB",
				&librdf_storage_audiodb_register_factory);
}

#else

void librdf_init_storage_audiodb(librdf_world *world) {
		librdf_storage_register_factory(world, "audiodb", "AudioDB",
				&librdf_storage_audiodb_register_factory);
}

#endif


