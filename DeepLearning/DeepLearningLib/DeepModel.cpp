#include "DeepModel.h"

#include <array>
#include <assert.h>

namespace deep_learning_lib
{
    using namespace concurrency;

    DataLayer::DataLayer(int depth, int width, int height)
        : data_(depth * width * height), data_view_(depth, width, height, data_)
    {
    }


    ConvolveLayer::ConvolveLayer(int num_neuron, int neuron_depth, int neuron_width, int neuron_height)
        : weights_(num_neuron * neuron_depth * neuron_width * neuron_height),
        weight_view_(extent<4>(std::array<int, 4>{{ num_neuron, neuron_depth, neuron_width, neuron_height }}.data()), weights_)
    {
    }

    void ConvolveLayer::PassUp(concurrency::array_view<const float, 3> bottom_layer,
        concurrency::array_view<float, 3> top_layer) const
    {
        assert(top_layer.extent[0] /* depth */ == this->neuron_num());

        // readonly
        array_view<const float, 4> neuron_weights = weight_view_;
        // writeonly
        top_layer.discard_data();

        // non-tiled version
        parallel_for_each(top_layer.extent,
            [=](index<3> idx) restrict(amp)
        {
            array_view<const float, 3> current_neuron = neuron_weights[idx[0]];// projection
            float result = 0.0f;

            for (int depth_idx = 0; depth_idx < current_neuron.extent[0]; depth_idx++)
            {
                for (int width_idx = 0; width_idx < current_neuron.extent[1]; width_idx++)
                {
                    for (int height_idx = 0; height_idx < current_neuron.extent[2]; height_idx++)
                    {
                        index<3> neuron_idx(depth_idx, width_idx, height_idx);
                        result += bottom_layer[idx + neuron_idx] * current_neuron[neuron_idx];
                    }
                }
            }

            top_layer[idx] = result;
        });
    }

    void ConvolveLayer::PassDown(concurrency::array_view<const float, 3> top_layer,
        concurrency::array_view<float, 3> bottom_layer) const
    {
        assert(top_layer.extent[0] == this->neuron_num());

        // readonly
        array_view<const float, 4> neuron_weights = weight_view_;
        // writeonly
        bottom_layer.discard_data();

        // non-tiled version
        parallel_for_each(bottom_layer.extent,
            [=](index<3> idx) restrict(amp)
        {
            float result = 0.0f;
            int cur_depth_idx = idx[0];
            int cur_width_idx = idx[1];
            int cur_height_idx = idx[2];

            for (int neuron_idx = 0; neuron_idx < neuron_weights.extent[0]; neuron_idx++)
            {
                array_view<const float, 3> current_neuron = neuron_weights[neuron_idx];

                for (int width_idx = 0; width_idx < neuron_weights.extent[2]; width_idx++)
                {
                    for (int height_idx = 0; height_idx < neuron_weights.extent[3]; height_idx++)
                    {
                        if (cur_width_idx - width_idx >= 0 && cur_height_idx - height_idx >= 0)
                        {
                            result += current_neuron(cur_depth_idx, cur_width_idx, cur_height_idx) * 
                                top_layer(neuron_idx, cur_width_idx - width_idx, cur_height_idx - height_idx);
                        }
                    }
                }
            }

            bottom_layer[idx] = result;
        });
    }
}
