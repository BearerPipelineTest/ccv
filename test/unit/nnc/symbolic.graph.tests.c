#include "case.h"
#include "ccv_case.h"
#include "ccv_nnc_case.h"
#include <ccv.h>
#include <nnc/ccv_nnc.h>
#include <nnc/ccv_nnc_easy.h>
#include "3rdparty/dsfmt/dSFMT.h"

TEST_SETUP()
{
	ccv_nnc_init();
}

TEST_CASE("compile symbolic graph of one node")
{
	ccv_nnc_symbolic_graph_t* symbolic_graph = ccv_nnc_symbolic_graph_new();
	ccv_nnc_tensor_symbol_t a = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "a");
	ccv_nnc_tensor_symbol_t b = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "b");
	ccv_nnc_tensor_symbol_t c = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "c");
	ccv_nnc_graph_exec_symbol_t prod = ccv_nnc_graph_exec_symbol_new(symbolic_graph, CMD_EWPROD_FORWARD(), TENSOR_SYMBOL_LIST(a, b), TENSOR_SYMBOL_LIST(c), "prod");
	ccv_nnc_graph_exec_symbol_autogen(symbolic_graph, 0, 0, CCV_NNC_AUTOGEN_ALL_EXECS | CCV_NNC_AUTOGEN_SOURCES_AND_DESTINATIONS);
	ccv_nnc_graph_t* graph = 0;
	ccv_nnc_tensor_arena_t* tensor_arena = 0;
	ccv_nnc_graph_exec_arena_t* graph_exec_arena = 0;
	ccv_nnc_symbolic_graph_compile(symbolic_graph, ccv_nnc_default_compile_params, 0, 0, 0, 0, GRAPH_EXEC_SYMBOL_LIST(prod), GRAPH_EXEC_SYMBOL_LIST(prod), &graph, &tensor_arena, &graph_exec_arena);
	SYMBOLIC_GRAPH_GEN(symbolic_graph, CCV_NNC_SHORT_DOT_GRAPH);
	GRAPH_GEN(graph, CCV_NNC_SHORT_DOT_GRAPH);
	ccv_nnc_tensor_t* a_tensor = ccv_nnc_tensor_from_symbol(tensor_arena, a);
	ccv_nnc_tensor_t* b_tensor = ccv_nnc_tensor_from_symbol(tensor_arena, b);
	ccv_nnc_tensor_t* c_tensor = ccv_nnc_tensor_from_symbol(tensor_arena, c);
	a_tensor->data.f32[0] = 1.2;
	b_tensor->data.f32[0] = 2.3;
	ccv_nnc_graph_exec_t prod_exec = ccv_nnc_graph_exec_from_symbol(graph_exec_arena, prod);
	ccv_nnc_graph_run(graph, 0, GRAPH_EXEC_LIST(prod_exec), GRAPH_EXEC_LIST(prod_exec), 0, 0);
	REQUIRE(a_tensor->data.f32 == c_tensor->data.f32, "trivially in-place operation, should point to the same memory region");
	REQUIRE_EQ_WITH_TOLERANCE(c_tensor->data.f32[0], 1.2 * 2.3, 1e-6, "should be equal to 1.2 * 2.3");
	ccv_nnc_graph_free(graph);
	ccv_nnc_tensor_arena_free(tensor_arena);
	ccv_nnc_graph_exec_arena_free(graph_exec_arena);
	ccv_nnc_symbolic_graph_free(symbolic_graph);
}

TEST_CASE("compile a simple symbolic graph with autogen")
{
	ccv_nnc_symbolic_graph_t* symbolic_graph = ccv_nnc_symbolic_graph_new();
	ccv_nnc_tensor_symbol_t a = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 31, 21, 2), "a");
	ccv_nnc_tensor_symbol_t b = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 31, 21, 4), "b");
	ccv_nnc_cmd_t forw_cmd = CMD_CONVOLUTION_FORWARD(1, 4, 5, 3, 2);
	ccv_nnc_tensor_symbol_t w = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 4, 5, 3, 2), "w");
	ccv_nnc_tensor_symbol_t bias = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 4), "bias");
	// See what we compile to when have unused tensors.
	ccv_nnc_tensor_symbol_t unused0 = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "unused0");
	ccv_nnc_tensor_symbol_t unused1 = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "unused1");
	ccv_nnc_graph_exec_symbol_new(symbolic_graph, forw_cmd, TENSOR_SYMBOL_LIST(a, w, bias), TENSOR_SYMBOL_LIST(b), "forw");
	ccv_nnc_tensor_symbol_t m = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 31, 21, 4), "m");
	ccv_nnc_cmd_t softmax_cmd = CMD_SOFTMAX_FORWARD();
	ccv_nnc_graph_exec_symbol_new(symbolic_graph, softmax_cmd, TENSOR_SYMBOL_LIST(b), TENSOR_SYMBOL_LIST(m), "softmax");
	ccv_nnc_graph_exec_symbol_autogen(symbolic_graph, 0, 0, CCV_NNC_AUTOGEN_ALL_EXECS | CCV_NNC_AUTOGEN_SOURCES_AND_DESTINATIONS);
	ccv_nnc_graph_t* graph = 0;
	ccv_nnc_tensor_arena_t* tensor_arena = 0;
	ccv_nnc_graph_exec_arena_t* graph_exec_arena = 0;
	ccv_nnc_symbolic_graph_compile(symbolic_graph, ccv_nnc_default_compile_params, 0, 0, 0, 0, SYMBOLIC_GRAPH_SOURCES(symbolic_graph), SYMBOLIC_GRAPH_DESTINATIONS(symbolic_graph), &graph, &tensor_arena, &graph_exec_arena);
	SYMBOLIC_GRAPH_GEN(symbolic_graph, CCV_NNC_LONG_DOT_GRAPH);
	GRAPH_GEN(graph, CCV_NNC_LONG_DOT_GRAPH);
	ccv_nnc_tensor_t* a_tensor = ccv_nnc_tensor_from_symbol(tensor_arena, a);
	ccv_nnc_tensor_t* b_tensor = ccv_nnc_tensor_from_symbol(tensor_arena, b);
	ccv_nnc_tensor_t* m_tensor = ccv_nnc_tensor_from_symbol(tensor_arena, m);
	REQUIRE(a_tensor->data.u8 != b_tensor->data.u8, "tensor a and b shouldn't share the memory.");
	REQUIRE(b_tensor->data.u8 == m_tensor->data.u8, "tensor b and m should share the memory because softmax is an inplace op.");
	REQUIRE(ccv_nnc_tensor_from_symbol(tensor_arena, unused0) == 0, "tensor unused 0 should have not pointed memory.");
	REQUIRE(ccv_nnc_tensor_from_symbol(tensor_arena, unused1) == 0, "tensor unused 0 should have not pointed memory.");
	ccv_nnc_symbolic_graph_free(symbolic_graph);
	ccv_nnc_graph_free(graph);
	ccv_nnc_tensor_arena_free(tensor_arena);
	ccv_nnc_graph_exec_arena_free(graph_exec_arena);
}

TEST_CASE("use symbolic graph disjoin and free")
{
	ccv_nnc_symbolic_graph_t* const symbolic_graph = ccv_nnc_symbolic_graph_new();
	ccv_nnc_tensor_symbol_t a = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "a");
	ccv_nnc_tensor_symbol_t b = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "b");
	ccv_nnc_tensor_symbol_t c = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "c");
	ccv_nnc_tensor_symbol_t x = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "x");
	ccv_nnc_tensor_symbol_t y = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "y");
	ccv_nnc_tensor_symbol_t z = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "z");
	ccv_nnc_tensor_symbol_t p = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "p");
	ccv_nnc_tensor_symbol_t q = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "q");
	ccv_nnc_graph_exec_symbol_t prod = ccv_nnc_graph_exec_symbol_new(symbolic_graph, CMD_EWPROD_FORWARD(), TENSOR_SYMBOL_LIST(x, y), TENSOR_SYMBOL_LIST(z), "prod");
	ccv_nnc_graph_exec_symbol_t log0 = ccv_nnc_graph_exec_symbol_new(symbolic_graph, CMD_EWLOG_FORWARD(), TENSOR_SYMBOL_LIST(p), TENSOR_SYMBOL_LIST(q), "log0");
	ccv_nnc_graph_exec_symbol_t sum = ccv_nnc_graph_exec_symbol_new(symbolic_graph, CMD_EWSUM_FORWARD(), TENSOR_SYMBOL_LIST(a, b), TENSOR_SYMBOL_LIST(c), "sum");
	ccv_nnc_graph_exec_symbol_concat(symbolic_graph, sum, prod);
	ccv_nnc_graph_exec_symbol_concat(symbolic_graph, sum, log0);
	ccv_nnc_graph_exec_symbol_disjoin(symbolic_graph, sum, prod);
	ccv_nnc_graph_exec_symbol_autogen(symbolic_graph, 0, 0, CCV_NNC_AUTOGEN_SOURCES_AND_DESTINATIONS);
	REQUIRE(ccv_nnc_symbolic_graph_source_size(symbolic_graph) == 2, "sources now should contain both sum and prod");
	REQUIRE(ccv_nnc_symbolic_graph_destination_size(symbolic_graph) == 2, "destinations now should contain both log0 and prod");
	ccv_nnc_graph_exec_symbol_free(symbolic_graph, prod);
	REQUIRE(ccv_nnc_symbolic_graph_source_size(symbolic_graph) == 1, "sources now should contain sum");
	REQUIRE(ccv_nnc_symbolic_graph_destination_size(symbolic_graph) == 1, "destinations now should contain log0");
	REQUIRE(ccv_nnc_symbolic_graph_sources(symbolic_graph)->d == sum.d, "source should be sum");
	REQUIRE(ccv_nnc_symbolic_graph_destinations(symbolic_graph)->d == log0.d, "source should be log0");
	SYMBOLIC_GRAPH_GEN(symbolic_graph, CCV_NNC_SHORT_DOT_GRAPH);
	ccv_nnc_symbolic_graph_free(symbolic_graph);
}

TEST_CASE("set tensor symbol shape after computation specified")
{
	ccv_nnc_symbolic_graph_t* const symbolic_graph = ccv_nnc_symbolic_graph_new();
	ccv_nnc_tensor_symbol_t a = ccv_nnc_tensor_symbol_new(symbolic_graph, ccv_nnc_tensor_auto, "a");
	ccv_nnc_tensor_symbol_t b = ccv_nnc_tensor_symbol_new(symbolic_graph, ccv_nnc_tensor_auto, "b");
	ccv_nnc_graph_exec_symbol_new(symbolic_graph, CMD_EWLOG_FORWARD(), TENSOR_SYMBOL_LIST(a), TENSOR_SYMBOL_LIST(b), "log");
	ccv_nnc_graph_exec_symbol_autogen(symbolic_graph, 0, 0, CCV_NNC_AUTOGEN_ALL_EXECS | CCV_NNC_AUTOGEN_SOURCES_AND_DESTINATIONS);
	ccv_nnc_tensor_symbol_set(symbolic_graph, a, CPU_TENSOR_NHWC(32F, 1));
	ccv_nnc_graph_t* graph = 0;
	ccv_nnc_tensor_arena_t* tensor_arena = 0;
	ccv_nnc_graph_exec_arena_t* graph_exec_arena = 0;
	ccv_nnc_symbolic_graph_compile(symbolic_graph, ccv_nnc_default_compile_params, 0, 0, 0, 0, SYMBOLIC_GRAPH_SOURCES(symbolic_graph), SYMBOLIC_GRAPH_DESTINATIONS(symbolic_graph), &graph, &tensor_arena, &graph_exec_arena);
	SYMBOLIC_GRAPH_GEN(symbolic_graph, CCV_NNC_LONG_DOT_GRAPH);
	ccv_nnc_symbolic_graph_free(symbolic_graph);
	GRAPH_GEN(graph, CCV_NNC_SHORT_DOT_GRAPH);
	ccv_nnc_tensor_t* const a_tensor = ccv_nnc_tensor_from_symbol(tensor_arena, a);
	a_tensor->data.f32[0] = 1.25;
	ccv_nnc_graph_run(graph, 0, TRAVERSE_FULL, 0, 0);
	ccv_nnc_tensor_t* const b_tensor = ccv_nnc_tensor_from_symbol(tensor_arena, b);
	REQUIRE_EQ_WITH_TOLERANCE(b_tensor->data.f32[0], logf(1.25), 1e-5, "should be equal to logf(1.25)");
	ccv_nnc_graph_free(graph);
	ccv_nnc_tensor_arena_free(tensor_arena);
	ccv_nnc_graph_exec_arena_free(graph_exec_arena);
}

TEST_CASE("query connectivity from one exec symbol to another")
{
	ccv_nnc_symbolic_graph_t* const symbolic_graph = ccv_nnc_symbolic_graph_new();
	ccv_nnc_tensor_symbol_t a = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "a");
	ccv_nnc_tensor_symbol_t b = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "b");
	ccv_nnc_graph_exec_symbol_t log1 = ccv_nnc_graph_exec_symbol_new(symbolic_graph, CMD_EWLOG_FORWARD(), TENSOR_SYMBOL_LIST(a), TENSOR_SYMBOL_LIST(b), "log1");
	ccv_nnc_tensor_symbol_t c = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "c");
	ccv_nnc_tensor_symbol_t d = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "d");
	ccv_nnc_graph_exec_symbol_t log2 = ccv_nnc_graph_exec_symbol_new(symbolic_graph, CMD_EWLOG_FORWARD(), TENSOR_SYMBOL_LIST(c), TENSOR_SYMBOL_LIST(d), "log2");
	ccv_nnc_tensor_symbol_t e = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "e");
	ccv_nnc_graph_exec_symbol_t sum1 = ccv_nnc_graph_exec_symbol_new(symbolic_graph, CMD_EWSUM_FORWARD(), TENSOR_SYMBOL_LIST(a, c), TENSOR_SYMBOL_LIST(e), "sum1");
	ccv_nnc_tensor_symbol_t f = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "f");
	ccv_nnc_graph_exec_symbol_t log3 = ccv_nnc_graph_exec_symbol_new(symbolic_graph, CMD_EWLOG_FORWARD(), TENSOR_SYMBOL_LIST(d), TENSOR_SYMBOL_LIST(f), "log3");
	ccv_nnc_tensor_symbol_t g = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 1), "g");
	ccv_nnc_graph_exec_symbol_t sum2 = ccv_nnc_graph_exec_symbol_new(symbolic_graph, CMD_EWSUM_FORWARD(), TENSOR_SYMBOL_LIST(b, f), TENSOR_SYMBOL_LIST(g), "sum2");
	ccv_nnc_graph_exec_symbol_autogen(symbolic_graph, 0, 0, CCV_NNC_AUTOGEN_ALL_EXECS | CCV_NNC_AUTOGEN_SOURCES_AND_DESTINATIONS);
	SYMBOLIC_GRAPH_GEN(symbolic_graph, CCV_NNC_LONG_DOT_GRAPH);
	uint64_t bitmask = 1;
	ccv_nnc_symbolic_graph_sources_to_destinations(symbolic_graph, GRAPH_EXEC_SYMBOL_LIST(sum1, log1, log2, log1), GRAPH_EXEC_SYMBOL_LIST(sum2), &bitmask);
	REQUIRE(bitmask == 14, "log1 and log2 should be sources for sum2, not sum1");
	bitmask = 0;
	ccv_nnc_symbolic_graph_sources_to_destinations(symbolic_graph, GRAPH_EXEC_SYMBOL_LIST(sum1, log1, log2), GRAPH_EXEC_SYMBOL_LIST(sum1), &bitmask);
	REQUIRE(bitmask == 1, "log1 and log2 are not sources for sum1");
	bitmask = 0;
	ccv_nnc_symbolic_graph_sources_to_destinations(symbolic_graph, GRAPH_EXEC_SYMBOL_LIST(log3, log1, log2), GRAPH_EXEC_SYMBOL_LIST(sum2), &bitmask);
	REQUIRE(bitmask == 7, "log3, log1 and log2 are not sources for sum2");
	ccv_nnc_symbolic_graph_free(symbolic_graph);
}

typedef struct {
	int called;
	int softmax_called;
	int convolution_called;
	int softmax_incomings;
	int softmax_outgoings;
	int convolution_incomings;
	int convolution_outgoings;
} format_stats_t;

static void _format_fn(const int node, const char* const name, const ccv_nnc_cmd_t cmd, const int flags, const int* const incomings, const int incoming_size, const int* const outgoings, const int outgoing_size, const int* const inputs, const int input_size, const int* const outputs, const int output_size, void* const context)
{
	format_stats_t* const stats = (format_stats_t*)context;
	++stats->called;
	if (cmd.cmd == CCV_NNC_CONVOLUTION_FORWARD)
	{
		++stats->convolution_called;
		stats->convolution_incomings = incoming_size;
		stats->convolution_outgoings = outgoing_size;
	} else if (cmd.cmd == CCV_NNC_SOFTMAX_FORWARD) {
		++stats->softmax_called;
		stats->softmax_incomings = incoming_size;
		stats->softmax_outgoings = outgoing_size;
	}
}

TEST_CASE("build a simple symbolic graph and check its format")
{
	ccv_nnc_symbolic_graph_t* symbolic_graph = ccv_nnc_symbolic_graph_new();
	ccv_nnc_tensor_symbol_t a = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 31, 21, 2), "a");
	ccv_nnc_tensor_symbol_t b = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 31, 21, 4), "b");
	ccv_nnc_cmd_t forw_cmd = CMD_CONVOLUTION_FORWARD(1, 4, 5, 3, 2);
	ccv_nnc_tensor_symbol_t w = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 4, 5, 3, 2), "w");
	ccv_nnc_tensor_symbol_t bias = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 4), "bias");
	ccv_nnc_graph_exec_symbol_new(symbolic_graph, forw_cmd, TENSOR_SYMBOL_LIST(a, w, bias), TENSOR_SYMBOL_LIST(b), "forw");
	ccv_nnc_tensor_symbol_t m = ccv_nnc_tensor_symbol_new(symbolic_graph, CPU_TENSOR_NHWC(32F, 31, 21, 4), "m");
	ccv_nnc_cmd_t softmax_cmd = CMD_SOFTMAX_FORWARD();
	ccv_nnc_graph_exec_symbol_new(symbolic_graph, softmax_cmd, TENSOR_SYMBOL_LIST(b), TENSOR_SYMBOL_LIST(m), "softmax");
	ccv_nnc_graph_exec_symbol_autogen(symbolic_graph, 0, 0, CCV_NNC_AUTOGEN_ALL_EXECS | CCV_NNC_AUTOGEN_SOURCES_AND_DESTINATIONS);
	SYMBOLIC_GRAPH_GEN(symbolic_graph, CCV_NNC_LONG_DOT_GRAPH);
	format_stats_t stats = {
		.called = 0,
		.softmax_called = 0,
		.convolution_called = 0,
		.softmax_incomings = 0,
		.softmax_outgoings = 0,
		.convolution_incomings = 0,
		.convolution_outgoings = 0,
	};
	ccv_nnc_symbolic_graph_format(symbolic_graph, 0, 0, 0, 0, _format_fn, &stats);
	REQUIRE(stats.called == 2, "called twice");
	REQUIRE(stats.convolution_called == 1, "called convolution");
	REQUIRE(stats.softmax_called == 1, "called softmax");
	REQUIRE(stats.convolution_incomings == 0, "convolution has no incomings");
	REQUIRE(stats.convolution_outgoings == 1, "convolution has 1 outgoing");
	REQUIRE(stats.softmax_incomings == 1, "softmax has 1 incoming");
	REQUIRE(stats.softmax_outgoings == 0, "softmax has no outgoings");
	ccv_nnc_symbolic_graph_free(symbolic_graph);
}

#include "case_main.h"
