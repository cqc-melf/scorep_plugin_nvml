#include "../lib/scorep_plugin_cxx_wrapper/include/scorep/chrono/time_convert.hpp"
#include "nvml_wrapper.hpp"

#include <scorep/plugin/plugin.hpp>

#include <nvml.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <chrono>

using namespace scorep::plugin::policy;

using scorep::plugin::logging;

struct nvml_t {
    nvml_t(const std::string& name_, unsigned int device_id_, Sampling_Metric* metric_)
        : name(name_), device_id(device_id_), metric(metric_)
    {
    }
    ~nvml_t()
    {
        logging::info() << "call destructor of " << name;
    }
    std::string name;
    unsigned int device_id;
    Sampling_Metric* metric;
};

template <typename T, typename Policies>
using nvml_object_id = scorep::plugin::policy::object_id<nvml_t, T, Policies>;

class nvml_sampling_plugin
    : public scorep::plugin::base<nvml_plugin, async, per_host, scorep_clock, frequent, nvml_object_id> {
public:
    nvml_sampling_plugin()
    {
        nvmlReturn_t nvml = nvmlInit_v2();
        if (NVML_SUCCESS != nvml) {
            throw std::runtime_error("Could not start NVML. Code: " +
                                     std::string(nvmlErrorString(nvml)));
        }
        nvml_devices = get_visible_devices();
    }

    ~nvml_sampling_plugin()
    {
        nvmlReturn_t nvml = nvmlShutdown();
        if (NVML_SUCCESS != nvml) {
            //            throw std::runtime_error("Could not terminate NVML. Code: " +
            //                                     std::string(nvmlErrorString(nvml)));
            logging::warn() << "Could not terminate NVML. Code:"
                            << std::string(nvmlErrorString(nvml));
        }
    }

    // Convert a named metric (may contain wildcards or so) to a vector of
    // actual metrics (may have a different name)
    std::vector<scorep::plugin::metric_property> get_metric_properties(const std::string& metric_name)
    {
        std::vector<scorep::plugin::metric_property> properties;

        logging::info() << "get metric properties called with: " << metric_name;

        Nvml_Sampling_Metric* metric_type = metric_name_2_nvml_sampling_function(metric_name);

        for (unsigned int i = 0; i < nvml_devices.size(); ++i) {
            /* TODO use device index by nvmlDeviceGetIndex( nvmlDevice_t device, unsigned int* index ) */

            std::string new_name = metric_name + " on CUDA: " + std::to_string(i);
            auto handle = make_handle(new_name, nvml_t{metric_name, i, metric_type});

            scorep::plugin::metric_property property = scorep::plugin::metric_property(
                new_name, metric_type->getDesc(), metric_type->getUnit());

            metric_datatype datatype = metric_type->getDatatype();
            switch (datatype) {
            case metric_datatype::UINT:
                property.value_uint();
                break;
            case metric_datatype::INT:
                property.value_int();
                break;
            case metric_datatype::DOUBLE:
                property.value_double();
                break;
            default:
                throw std::runtime_error("Unknown datatype for metric " + metric_name);
            }

            metric_measure_type measure_type = metric_type->getMeasureType();
            switch (measure_type) {
            case metric_measure_type::ABS:
                property.absolute_point();
                break;
            case metric_measure_type::REL:
                property.relative_point();
                break;
            case metric_measure_type::ACCU:
                property.accumulated_point();
                break;
            default:
                throw std::runtime_error("Unknown measure type for metric " + metric_name);
            }

            properties.push_back(property);
        }
        return properties;
    }

    static uint64_t get_metric_gather_interval()
    {
        logging::info() << "get_metric_gather_interval called";
        unsigned long interval = std::stoul(scorep::environment_variable::get("INTERVAL", "5"));
        return interval / CLOCKS_PER_SEC;
    }

    void add_metric(nvml_t& handle)
    {
        logging::info() << "add metric called with: " << handle.name
                        << " on CUDA " << handle.device_id;
    }

    // start your measurement in this method
    void start()
    {
        begin = scorep::chrono::measurement_clock::now();
        convert.synchronize_point();
    }

    // stop your measurement in this method
    void stop()
    {
        end = scorep::chrono::measurement_clock::now();
        convert.synchronize_point();

        //        for (auto& handle : get_handles())
        //        {
        //            handle.data = get_value(handle.metric, nvml_devices[handle.device_id]);
        //        }
        logging::info() << "stopp called";
    }

    // Will be called post mortem by the measurement environment
    // You return all values measured.
    template <typename C>
    void get_all_values(nvml_t& handle, C& cursor)
    {
        logging::info() << "get_all_values called with: " << handle.name
                        << " CUDA " << handle.device_id;

        unsigned long long last_seen = begin.count();
        std::vector<pair_time_sampling_t> data = handle.metric->get_value(nvml_devices[handle.device_id], last_seen);
        for (auto pair : data)
        {
            //const auto time = chrono::system
            cursor.write(convert.to_ticks(pair.first), pair.second);
        }
    }

private:
    std::vector<nvmlDevice_t> nvml_devices;
    scorep::chrono::ticks begin, end;
    scorep::chrono::time_convert<> convert;

private:
    std::vector<nvmlDevice_t> get_visible_devices()
    {
        std::vector<nvmlDevice_t> devices;

        nvmlReturn_t ret;
        unsigned int num_devices;

        ret = nvmlDeviceGetCount(&num_devices);
        if (NVML_SUCCESS != ret) {
            throw std::runtime_error(nvmlErrorString(ret));
        }

        /*
         * New nvmlDeviceGetCount_v2 (default in NVML 5.319) returns count of all devices in the system
         * even if nvmlDeviceGetHandleByIndex_v2 returns NVML_ERROR_NO_PERMISSION for such device.
         */
        nvmlDevice_t device;
        for (unsigned i = 0; i < num_devices; ++i) {
            ret = nvmlDeviceGetHandleByIndex(i, &device);

            if (NVML_SUCCESS == ret) {
                devices.push_back(device);
            }
            else if (NVML_ERROR_NO_PERMISSION == ret) {
                logging::info() << "No permission for device: " << i;
            }
            else {
                throw std::runtime_error(nvmlErrorString(ret));
            }
        }
        return devices;
    }
};