#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h> /* for abort() as used in errors */
#endif
#include <sys/types.h>
#include <librdf.h>
#include <audioDB_API.h>

/**
 * NB : This is pulled through from librdf internals. Not ideal, but
 * otherwise we'd need to compile against their source tree!
 **/

#define LIBRDF_SIGN_KEY 0x04Ed1A7D

#define AF_DIMENSION "http://purl.org/ontology/af/dimension"
#define AF_VECTORS "http://purl.org/ontology/af/vectors"
#define MO_SIGNAL "http://purl.org/ontology/mo/Signal"
#define RDF_TYPE "http://www.w3.org/1999/02/22-rdf-syntax-ns#type"


typedef struct {
	librdf_model* model;
	librdf_storage* storage;
	char* name;
	size_t name_len;
	int is_new;

	adb_t* adb;

} librdf_storage_audiodb_instance;


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
void librdf_storage_module_register_factory(librdf_world *world);

/**
 * These 3 are from librdf's rdf_internals - simplify the mallocing/freeing a bit.
 */

void librdf_sign_free(void *ptr)
{
	int *p;

	if(!ptr)
		return;

	p=(int*)ptr;
	p--;

	if(*p != LIBRDF_SIGN_KEY)
		return;

	free(p);
}


void* librdf_sign_calloc(size_t nmemb, size_t size)
{
	int *p;

	/* turn into bytes */
	size = nmemb*size + sizeof(int);

	p=(int*)calloc(1, size);
	*p++ = LIBRDF_SIGN_KEY;
	return p;
}

void* librdf_sign_malloc(size_t size)
{
	int *p;

	size += sizeof(int);

	p=(int*)malloc(size);
	*p++ = LIBRDF_SIGN_KEY;
	return p;
}


static int librdf_storage_audiodb_init(librdf_storage* storage, const char *name, librdf_hash* options) {
	
	librdf_storage_audiodb_instance* context;
	char* name_copy;

	context = (librdf_storage_audiodb_instance*)librdf_sign_calloc(1, sizeof(librdf_storage_audiodb_instance));

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
	name_copy=(char*)librdf_sign_malloc(context->name_len+1);
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

	librdf_log(librdf_storage_get_world(storage), 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
		"Initialised!");

	return 0;
}

static int librdf_storage_audiodb_open(librdf_storage* storage, librdf_model* model) {
	librdf_storage_audiodb_instance* context = (librdf_storage_audiodb_instance*)librdf_storage_get_instance(storage);
	int db_file_exists = 0;

	librdf_log(librdf_storage_get_world(storage), 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
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
			librdf_log(librdf_storage_get_world(storage), 0, LIBRDF_LOG_ERROR, LIBRDF_FROM_STORAGE, NULL,
					"Unable to create %s", context->name);
			return 1;
		}
	}
	else
	{
		if(!(context->adb = audiodb_open(context->name, O_RDWR))) {	
			librdf_log(librdf_storage_get_world(storage), 0, LIBRDF_LOG_ERROR, LIBRDF_FROM_STORAGE, NULL,
					"Unable to open %s", context->name);
			return 1;
		}
	}

	return 0;
}

static int librdf_storage_audiodb_close(librdf_storage* storage) {
	librdf_storage_audiodb_instance* context = (librdf_storage_audiodb_instance*)librdf_storage_get_instance(storage);	
	
	librdf_log(librdf_storage_get_world(storage), 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
	"close");

	if(context->adb)
	{
		audiodb_close(context->adb);
		context->adb = NULL;
	}

	return 0;
}

static int librdf_storage_audiodb_size(librdf_storage* storage) {
	librdf_log(librdf_storage_get_world(storage), 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
		"size");
	return 0;
}

static int librdf_storage_audiodb_add_statement(librdf_storage* storage, librdf_statement* statement) {
	librdf_log(librdf_storage_get_world(storage), 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
	"add statement");
	return 0;
}

static int librdf_storage_audiodb_add_statements(librdf_storage* storage, librdf_stream* statement_stream) {
	librdf_log(librdf_storage_get_world(storage), 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
	"add statements");
	return 0;
}

static int librdf_storage_audiodb_remove_statement(librdf_storage* storage, librdf_statement* statement) {
	librdf_log(librdf_storage_get_world(storage), 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
	"remove statement");
	return 0;
}

static int librdf_storage_audiodb_contains_statement(librdf_storage* storage, librdf_statement* statement) {

	librdf_storage_audiodb_instance* context = (librdf_storage_audiodb_instance*)librdf_storage_get_instance(storage);	
	librdf_world* world = librdf_storage_get_world(storage);
	
	librdf_node* subject = librdf_statement_get_subject(statement);
	librdf_node* object = librdf_statement_get_object(statement);
	librdf_node* predicate = librdf_statement_get_predicate(statement);
	librdf_log(world, 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
		"Contains statement %s?", librdf_statement_to_string(statement));


	if(subject && object && predicate)
	{
		// audioDBs only contain Signals (tracks are held in a separate store).

		librdf_uri* type_uri = librdf_new_uri(world, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
		librdf_uri* signal_uri = librdf_new_uri(world, "http://purl.org/ontology/mo/Signal");
		if(librdf_uri_equals(type_uri, librdf_node_get_uri(predicate)))
		{
			librdf_uri* object_uri = librdf_node_get_uri(object);
			if(librdf_uri_equals(object_uri, signal_uri))
			{
				// Grab the track via audioDB
				adb_datum_t datum = {0};
				int result = audiodb_retrieve_datum(
					context->adb, 
					librdf_uri_as_string(librdf_node_get_uri(subject)),
					&datum);
				if(result == 0)
				{
					// Found it! Free up the datum.
					audiodb_free_datum(context->adb, &datum);
					return 1;
				}
			}
		}
		librdf_free_uri(type_uri);
		librdf_free_uri(signal_uri);
	}
	return 0;
}

static librdf_stream* librdf_storage_audiodb_serialise(librdf_storage* storage) {
	librdf_log(librdf_storage_get_world(storage), 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
	"serialise");
	return NULL;
}

/**
 * Linked list bits
 */


struct list_node_s
{
	struct list_node_s* next;
	struct list_node_s* prev;
	librdf_statement* statement;
};

typedef struct list_node_s list_node;

struct list_s {
	list_node* head;
	list_node* tail;
};

typedef struct list_s result_list;

result_list* result_data_new()
{
	result_list* list = (result_list*)calloc(1, sizeof(result_list));
	if(!list)
		return NULL;
	return list;
}

int result_data_add(librdf_world* world, result_list* list) {

	// First create the node
	list_node* node = (list_node*)calloc(1, sizeof(list_node));

	if(!node)
		return 1;
	
	if(list->tail)
	{
		node->prev = list->tail;
		list->tail->next = node;
	}

	list->tail = node;
	if(!list->head)
		list->head = node;
	return 0;
}

/**
 * Querying bits.
 **/

typedef struct {
	librdf_storage* storage;
	librdf_storage_audiodb_instance* audiodb_context;
	int finished;
	librdf_statement* statement;
	librdf_statement* query_statement;
	librdf_node* context;
	result_list *results;

} librdf_storage_audiodb_find_statements_stream_context;




static librdf_stream* librdf_storage_audiodb_find_statements(librdf_storage* storage, librdf_statement* statement) {

	librdf_storage_audiodb_instance* context = (librdf_storage_audiodb_instance*)librdf_storage_get_instance(storage);	
	librdf_storage_audiodb_find_statements_stream_context* scontext;
	librdf_stream* stream;


	librdf_world* world = librdf_storage_get_world(storage);

	librdf_log(librdf_storage_get_world(storage), 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
			"find statements %s", librdf_statement_to_string(statement));

	// Create stream context 
	scontext = (librdf_storage_audiodb_find_statements_stream_context*)librdf_sign_calloc(1, sizeof(librdf_storage_audiodb_find_statements_stream_context));

	if(!scontext)
		return NULL;

	scontext->storage = storage;
	librdf_storage_add_reference(scontext->storage);
	
	// Store a reference to the storage instance.
	scontext->audiodb_context = context;

	// Clone the query statement and store away.
	scontext->query_statement = librdf_new_statement_from_statement(statement);
	if(!scontext->query_statement) {
		librdf_storage_audiodb_find_statements_finished((void*)scontext);
		return NULL;
	}
	scontext->results = result_data_new();

	// This will need factoring out
	librdf_node* subject = librdf_statement_get_subject(statement);
	librdf_node* object = librdf_statement_get_object(statement);
	librdf_node* predicate = librdf_statement_get_predicate(statement);

	bool subjres = (subject && librdf_node_is_resource(subject)); 
	bool predres = (predicate && librdf_node_is_resource(predicate)); 
	bool objres = (object && librdf_node_is_resource(object)); 

	librdf_uri* dimension = librdf_new_uri(world, AF_DIMENSION);
	librdf_uri* vectors = librdf_new_uri(world, AF_VECTORS);
	librdf_uri* signal = librdf_new_uri(world, MO_SIGNAL);
	librdf_uri* type = librdf_new_uri(world, RDF_TYPE);

	if(subjres && predres)
	{
		librdf_log(librdf_storage_get_world(storage), 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL, "Got SPX");
		librdf_uri* predicate_uri = librdf_node_get_uri(predicate);
		librdf_uri* subject_uri = librdf_node_get_uri(subject);
		if(librdf_uri_equals(predicate_uri, dimension) || librdf_uri_equals(predicate_uri, vectors)) 
		{
			// Need dimension or vectors - so get the track datum.
			adb_datum_t datum = {0};
			int result = audiodb_retrieve_datum(
				context->adb, 
				librdf_uri_as_string(subject_uri),
				&datum);
			if(result == 0)
			{
				librdf_node* value;
				raptor_stringbuffer* buffer = raptor_new_stringbuffer();
				
				if(librdf_uri_equals(predicate_uri, dimension))
					raptor_stringbuffer_append_decimal(buffer, datum.dim);
				else if(librdf_uri_equals(predicate_uri, vectors))
					raptor_stringbuffer_append_decimal(buffer, datum.nvectors);

				value = librdf_new_node_from_typed_literal(world, raptor_stringbuffer_as_string(buffer), NULL, librdf_new_uri(world, "http://www.w3.org/2001/XMLSchema#integer"));

				result_data_add(world, scontext->results);
				
				result_list* list = scontext->results;
				
				list->tail->statement = librdf_new_statement(world);
				librdf_statement_set_subject(list->tail->statement, subject); 
				librdf_statement_set_object(list->tail->statement, value);
				librdf_statement_set_predicate(list->tail->statement, predicate);
				
				librdf_log(world, 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL, "Add statement %s", librdf_statement_to_string(list->tail->statement)); 
			}

		}
	}
	else if(predres && objres)
	{
		// Got XPO
		librdf_log(world, 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL, "XPO"); 
		librdf_uri* predicate_uri = librdf_node_get_uri(predicate);
		librdf_uri* object_uri = librdf_node_get_uri(object);

		if(librdf_uri_equals(predicate_uri, type) && librdf_uri_equals(object_uri, signal))
		{
			adb_liszt_results_t* liszt_results = audiodb_liszt(context->adb);

			uint32_t k;
			for(k = 0; k < liszt_results->nresults; k++) {
				result_data_add(world, scontext->results);
				result_list* list = scontext->results;
				list->tail->statement = librdf_new_statement(world);
				librdf_statement_set_subject(list->tail->statement, librdf_new_node_from_uri(world, librdf_new_uri(world, liszt_results->entries[k].key))); 

				librdf_statement_set_predicate(list->tail->statement, predicate);
				librdf_statement_set_object(list->tail->statement, object);
				librdf_log(world, 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL, "Add statement %s", librdf_statement_to_string(list->tail->statement)); 
			}
			
			audiodb_liszt_free_results(context->adb, liszt_results);
			
		}
	}
	else
	{
		librdf_log(world, 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL, "Halp?"); 
	}
	librdf_free_uri(dimension);
	librdf_free_uri(vectors);
	librdf_free_uri(signal);
	librdf_free_uri(type);
	
	stream = librdf_new_stream(world,
		(void*)scontext,
		&librdf_storage_audiodb_find_statements_end_of_stream,
		&librdf_storage_audiodb_find_statements_next_statement,
		&librdf_storage_audiodb_find_statements_get_statement,
		&librdf_storage_audiodb_find_statements_finished);

	if(!stream) {
		librdf_log(world, 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
			"Couldn't create stream!");
		librdf_storage_audiodb_find_statements_finished((void*)scontext);
		return NULL;
	}

	return stream;
}

static void librdf_storage_audiodb_find_statements_finished(void* context) {
	librdf_storage_audiodb_find_statements_stream_context* scontext=(librdf_storage_audiodb_find_statements_stream_context*)context;
		
	librdf_log(librdf_storage_get_world(scontext->storage), 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
		"Finished!");

	if(scontext->storage)
		librdf_storage_remove_reference(scontext->storage);

	if(scontext->query_statement)
		librdf_free_statement(scontext->query_statement);

	if(scontext->statement)
		librdf_free_statement(scontext->statement);

	if(scontext->context)
		librdf_free_node(scontext->context);

	librdf_sign_free(scontext);
}

static int librdf_storage_audiodb_find_statements_end_of_stream(void* context) {
	librdf_storage_audiodb_find_statements_stream_context* scontext=(librdf_storage_audiodb_find_statements_stream_context*)context;
	librdf_world* world = librdf_storage_get_world(scontext->storage);
	librdf_log(librdf_storage_get_world(scontext->storage), 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
		"Finished? %d", (scontext->results->head == NULL));

	return (scontext->results->head == NULL);
}

static int librdf_storage_audiodb_find_statements_next_statement(void* context) {
	librdf_storage_audiodb_find_statements_stream_context* scontext=(librdf_storage_audiodb_find_statements_stream_context*)context;

	librdf_world* world = librdf_storage_get_world(scontext->storage);
	librdf_log(librdf_storage_get_world(scontext->storage), 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
		"to next");

	if(scontext->results->head->next)
	{
		scontext->results->head = scontext->results->head->next;
		return 0; 
	}
	else
	{
		return 1;
	}
}

static void* librdf_storage_audiodb_find_statements_get_statement(void* context, int flags) {
	librdf_storage_audiodb_find_statements_stream_context* scontext=(librdf_storage_audiodb_find_statements_stream_context*)context;
	librdf_world* world = librdf_storage_get_world(scontext->storage);
	librdf_log(librdf_storage_get_world(scontext->storage), 0, LIBRDF_LOG_INFO, LIBRDF_FROM_STORAGE, NULL,
		"Get next");
	
	switch(flags) {
		case LIBRDF_ITERATOR_GET_METHOD_GET_OBJECT:
			return scontext->results->head->statement;
		case LIBRDF_ITERATOR_GET_METHOD_GET_CONTEXT:
			return scontext->context;
		default:
			librdf_log(world,
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

/** Entry point for dynamically loaded storage module */
void librdf_storage_module_register_factory(librdf_world *world) {
		librdf_storage_register_factory(world, "audiodb", "AudioDB",
				&librdf_storage_audiodb_register_factory);
}
