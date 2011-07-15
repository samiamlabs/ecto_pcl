/*
 * Copyright (c) 2011, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "ecto_pcl.hpp"
#include <pcl/filters/voxel_grid.h>

struct VoxelGrid
{
  #define DECLAREVOXELGRID(r, data, i, ELEM)    \
    BOOST_PP_COMMA_IF(i) BOOST_PP_CAT(pcl::VoxelGrid<pcl::Point,ELEM)>
  typedef boost::variant< BOOST_PP_SEQ_FOR_EACH_I(DECLAREVOXELGRID, ~, POINTTYPES) > filter_variant_t;

  /* used to create a filter */
  template <template <class> class FilterPolicy>
  struct make_filter_variant : boost::static_visitor<filter_variant_t>
  {
    template <typename CloudType >
    filter_variant_t operator()(const CloudType& p) const
    {
      return filter_variant_t(FilterPolicy<typename CloudType::element_type::PointType>());
    }
  };

  /* used to configure filter */
  struct filter_params { std::string filter_field_name; double filter_limit_min; double filter_limit_max; bool filter_limit_negative; double leaf_size; };
  struct filter_configurator : boost::static_visitor<void>
  { 
    filter_params& fp;
    filter_configurator(filter_params& fp_) : fp(fp_) {}

    template <typename PointType>
    void operator()(pcl::VoxelGrid<PointType>& impl_) const
    {
      impl_.setFilterFieldName(fp.filter_field_name);
      impl_.setFilterLimits(fp.filter_limit_min, fp.filter_limit_max);
      impl_.setFilterLimitsNegative(fp.filter_limit_negative);
      impl_.setLeafSize(fp.leaf_size, fp.leaf_size, fp.leaf_size);
    }
  };

  /* dispatch to handle process */
  struct filter_dispatch : boost::static_visitor<cloud_variant_t>
  {
    template <typename Filter, typename CloudType>
    cloud_variant_t operator()(Filter& f, CloudType& i) const
    {
      return impl(f, i, filter_takes_point_trait<Filter, CloudType>());
    }

    template <typename Filter, typename CloudType>
    cloud_variant_t impl(Filter& f, boost::shared_ptr<const CloudType>& i, boost::true_type) const
    {
      CloudType o;
      f.setInputCloud(i);
      f.filter(o);
      return cloud_variant_t(o.makeShared());
    }

    template <typename Filter, typename CloudType>
    cloud_variant_t impl(Filter& f, CloudType& i, boost::false_type) const
    {
      throw std::runtime_error("types aren't the same, you are doing something baaaaaad");
    }
  };

  static void declare_params(ecto::tendrils& params)
  {
    // filter params
    pcl::VoxelGrid<pcl::PointXYZRGB> default_;
    params.declare<std::string> ("filter_field_name", "The name of the field to use for filtering.", "");
    double filter_limit_min, filter_limit_max;
    default_.getFilterLimits(filter_limit_min, filter_limit_max);
    params.declare<double> ("filter_limit_min", "Minimum value for the filter.", filter_limit_min);
    params.declare<double> ("filter_limit_max", "Maximum value for the filter.", filter_limit_max);
    params.declare<bool> ("filter_limit_negative", "To negate the limits or not.", default_.getFilterLimitsNegative());

    // custom params
    params.declare<float> ("leaf_size", "The size of the leaf(meters), smaller means more points...", 0.05);
  }

  static void declare_io(const tendrils& params, tendrils& inputs, tendrils& outputs)
  {
    inputs.declare<PointCloud> ("input", "The cloud to filter");
    outputs.declare<PointCloud> ("output", "Filtered cloud.");
  }

  VoxelGrid() {}

  void configure(tendrils& params, tendrils& inputs, tendrils& outputs)
  {
    // set in/out.
    input_ = inputs.at("input");
    output_ = outputs.at("output");

    // instantiate and set filter details.
    filter_params fp;
    fp.filter_field_name = params.get<std::string> ("filter_field_name");
    fp.filter_limit_min = params.get<double> ("filter_limit_min");
    fp.filter_limit_max = params.get<double> ("filter_limit_max");
    fp.filter_limit_negative = params.get<bool> ("filter_limit_negative");
    fp.leaf_size = params.get<float> ("leaf_size");
    
    cloud_variant_t cv = input_->make_variant();
    if(!configured_){
      impl_ = boost::apply_visitor(make_filter_variant<pcl::VoxelGrid>(), cv);
      boost::apply_visitor(filter_configurator(fp), impl_);
      configured_ = true;
    }
  }

  int process(const tendrils& inputs, tendrils& outputs)
  {
    cloud_variant_t cvar = input_->make_variant();
    *output_ = boost::apply_visitor(filter_dispatch(), impl_, cvar);
    return 0;
  }

  bool configured_;
  filter_variant_t impl_;
  ecto::spore<PointCloud> input_;
  ecto::spore<PointCloud> output_;

};

ECTO_CELL(ecto_pcl, VoxelGrid, "VoxelGrid", "Voxel grid filter");


