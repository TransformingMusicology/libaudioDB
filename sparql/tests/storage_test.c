#include <librdf.h>

int main()
{
	librdf_world* world = librdf_new_world();
	librdf_storage* storage = librdf_new_storage(world, "audiodb", "test.adb", "new='yes'");

	librdf_model *model;
	if (!(model = librdf_new_model(world, storage, NULL)))
		goto librdf_error;

	librdf_storage_close(storage);
/*
	librdf_query *query;
	if (!(query = librdf_new_query(world, "sparql", NULL, "PREFIX abc: <http://example.com/exampleOntology#> SELECT ?capital ?country WHERE { ?x abc:cityname ?capital ; abc:isCapitalOf ?y .  ?y abc:countryname ?country ; abc:isInContinent abc:Africa .  }", NULL))) 
		goto librdf_error;

	librdf_query_results *results;
	if (!(results = librdf_query_execute(query, model)))
		goto librdf_error;

	if(!librdf_query_results_is_bindings(results))
		goto librdf_error;
*/
	return 0;

	librdf_error:
		printf("Wah!\n");
		return 1;
}
