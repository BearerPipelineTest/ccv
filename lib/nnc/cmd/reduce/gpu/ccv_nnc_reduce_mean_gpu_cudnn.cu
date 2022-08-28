extern "C" {
#include <ccv.h>
#include <ccv_internal.h>
#include <nnc/ccv_nnc.h>
#include <nnc/ccv_nnc_easy.h>
#include <nnc/ccv_nnc_internal.h>
}
#include <nnc/gpu/ccv_nnc_compat.h>

#ifdef HAVE_CUDNN

static int _ccv_nnc_reduce_mean_forw(const ccv_nnc_cmd_t cmd, const ccv_nnc_hint_t hint, const int flags, ccv_nnc_tensor_t* const* const inputs, const int input_size, ccv_nnc_tensor_t* const* const outputs, const int output_size, ccv_nnc_stream_context_t* const stream_context)
{
	assert(input_size == 1);
	cudnnHandle_t cudnn = ccv_nnc_stream_context_get_cudnn(stream_context);
	ccv_nnc_tensor_view_t atv = ccv_nnc_get_tensor_view(inputs[0]);
	const ccv_nnc_cudnn_tensor_view_descriptor_t a = ccv_nnc_cudnn_get_tensor_view_descriptor_for_op(stream_context, (const ccv_nnc_tensor_view_t*)inputs[0]);
	ccv_nnc_tensor_view_t btv = ccv_nnc_get_tensor_view(outputs[0]);
	ccv_nnc_tensor_view_t* tvs[] = {
		&atv, &btv
	};
	ccv_nnc_tensor_view_alignment(tvs, 2);
	const ccv_nnc_cudnn_tensor_view_descriptor_t b = ccv_nnc_cudnn_get_tensor_view_descriptor_for_op(stream_context, &btv);
	int can_reduce = 0;
	int i;
	for (i = 0; !can_reduce && i < cmd.info.reduce.count; i++)
		can_reduce = (inputs[0]->info.dim[cmd.info.reduce.axis[i]] > 1);
	static const float one = 1, zero = 0;
	if (can_reduce)
	{
		cudnnReduceTensorDescriptor_t reduce_mean = ccv_nnc_stream_context_get_reduce_tensor_descriptor(stream_context);
		cudnnSetReduceTensorDescriptor(reduce_mean, CUDNN_REDUCE_TENSOR_AVG, CUDNN_DATA_FLOAT, CUDNN_PROPAGATE_NAN, CUDNN_REDUCE_TENSOR_NO_INDICES, CUDNN_32BIT_INDICES);
		void* workspace = 0;
		size_t workspace_size = 0;
		CUDNN_ENFORCE(cudnnGetReductionWorkspaceSize(cudnn, reduce_mean, a.descriptor, b.descriptor, &workspace_size));
		if (workspace_size)
		{
			workspace = ccv_nnc_stream_context_get_workspace(stream_context, workspace_size, CCV_TENSOR_GPU_MEMORY);
			assert(workspace);
		}
		CUDNN_ENFORCE(cudnnReduceTensor(cudnn, reduce_mean, 0, 0, workspace, workspace_size, &one, a.descriptor, a.data.u8, &zero, b.descriptor, b.data.u8));
		ccv_nnc_stream_context_return_reduce_tensor_descriptor(stream_context, reduce_mean);
	} else if (a.data.u8 != b.data.u8) {
		// Don't need to reduce, just transfer to b, if the pointer doesn't match.
		CUDNN_ENFORCE(cudnnTransformTensor(cudnn, &one, a.descriptor, a.data.u8,  &zero, b.descriptor, b.data.u8));
	}
	ccv_nnc_cudnn_deinit_tensor_view_descriptor(a);
	ccv_nnc_cudnn_deinit_tensor_view_descriptor(b);
	return CCV_NNC_EXEC_SUCCESS;
}

static int _ccv_nnc_reduce_mean_back(const ccv_nnc_cmd_t cmd, const ccv_nnc_hint_t hint, const int flags, ccv_nnc_tensor_t* const* const inputs, const int input_size, ccv_nnc_tensor_t* const* const outputs, const int output_size, ccv_nnc_stream_context_t* const stream_context)
{
	cudnnHandle_t cudnn = ccv_nnc_stream_context_get_cudnn(stream_context);
	ccv_nnc_tensor_view_t* const atv = (ccv_nnc_tensor_view_t*)outputs[0];
	const ccv_nnc_cudnn_tensor_view_descriptor_t a = ccv_nnc_cudnn_get_tensor_view_descriptor_for_op(stream_context, atv);
	if (inputs[0] == 0)
	{
		int i;
		ssize_t dims = 1;
		for (i = 0; i < cmd.info.reduce.count; i++)
			dims *= outputs[0]->info.dim[cmd.info.reduce.axis[i]];
		const float scale = 1.0 / (float)dims;
		CUDNN_ENFORCE(cudnnSetTensor(cudnn, a.descriptor, a.data.u8, &scale));
	} else {
		static const float zero = 0;
		ccv_nnc_tensor_view_t gtv = ccv_nnc_get_tensor_view(inputs[0]);
		ccv_nnc_tensor_view_t* tvs[] = {
			atv, &gtv
		};
		ccv_nnc_tensor_view_alignment(tvs, 2);
		const ccv_nnc_cudnn_tensor_view_descriptor_t g = ccv_nnc_cudnn_get_tensor_view_descriptor_for_op(stream_context, &gtv);
		const float scale = (float)ccv_nnc_tensor_count(inputs[0]->info) / (float)ccv_nnc_tensor_count(outputs[0]->info);
		CUDNN_ENFORCE(cudnnAddTensor(cudnn, &scale, g.descriptor, g.data.u8, &zero, a.descriptor, a.data.u8));
		ccv_nnc_cudnn_deinit_tensor_view_descriptor(g);
	}
	ccv_nnc_cudnn_deinit_tensor_view_descriptor(a);
	return CCV_NNC_EXEC_SUCCESS;
}

#endif

REGISTER_COMMAND_BACKEND(CCV_NNC_REDUCE_MEAN_FORWARD, CCV_NNC_BACKEND_GPU_CUDNN)(ccv_nnc_cmd_backend_registry_t* const registry)
{
#ifdef HAVE_CUDNN
	registry->tensor_formats = CCV_TENSOR_FORMAT_NHWC | CCV_TENSOR_FORMAT_NCHW | CCV_TENSOR_FORMAT_CHWN;
	registry->tensor_datatypes = CCV_32F | CCV_16F;
	registry->tensor_memory = CCV_TENSOR_GPU_MEMORY;
	registry->algorithms = 1;
	registry->exec = _ccv_nnc_reduce_mean_forw;
#endif
}

REGISTER_COMMAND_BACKEND(CCV_NNC_REDUCE_MEAN_BACKWARD, CCV_NNC_BACKEND_GPU_CUDNN)(ccv_nnc_cmd_backend_registry_t* const registry)
{
#ifdef HAVE_CUDNN
	registry->tensor_formats = CCV_TENSOR_FORMAT_NHWC | CCV_TENSOR_FORMAT_NCHW | CCV_TENSOR_FORMAT_CHWN;
	registry->tensor_datatypes = CCV_32F | CCV_16F;
	registry->tensor_memory = CCV_TENSOR_GPU_MEMORY;
	registry->algorithms = 1;
	registry->exec = _ccv_nnc_reduce_mean_back;
#endif
}

