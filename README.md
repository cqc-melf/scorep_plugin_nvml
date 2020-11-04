# Score-P NVML Plugin
This code provides a Score-P plugin to access [NVIDIA Management Library (NVML)](https://developer.nvidia.com/nvidia-management-library-nvml)

## Installation
If not present on your system get NVML from [NVIDIA Management Library (NVML)](https://developer.nvidia.com/nvidia-management-library-nvml) and istall it with
`./gdk_linux_*_release.run --installdir=<PATH> --silent`
Set Paths
```
    git clone --recurse-submodules git@github.com:NanoNabla/scorep_plugin_nvml.git
    cd scorep_plugin_nvml
    mkdir build && cd build
    cmake ../
    make
```


## Usage
Sampling mode seems to be more efficient than async or sync plugin
### Sampling
- `SCOREP_METRIC_PLUGINS=nvml_sampling_plugin`
- `SCOREP_METRIC_NVML_SAMPLING_PLUGIN="power_usage,temperature"`
    
Optional :
- `SCOREP_METRIC_NVML_SAMPLING__PLUGIN_INTERVAL="3"` (Interval in seconds)

#### Available metrics
- `clock_sm`
- `clock_mem`
- `power_usage`
- `utilization_gpu`
- `utilization_mem`


## Developer note 
Current `nvml.h` can be found under 
https://github.com/NVIDIA/nvidia-settings/blob/master/src/nvml.h
