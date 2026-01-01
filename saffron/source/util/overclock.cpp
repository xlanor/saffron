// Ported from Switchfin: https://github.com/dragonflylee/switchfin
#include "util/overclock.hpp"

#ifdef __SWITCH__

#include <borealis.hpp>

int SwitchSys::stock_cpu_clock = 0;
int SwitchSys::stock_gpu_clock = 0;
int SwitchSys::stock_emc_clock = 0;
bool SwitchSys::initialized = false;

void SwitchSys::init() {
    if (initialized) return;

    bool is_old = hosversionBefore(8, 0, 0);

    if (is_old) {
        if (R_SUCCEEDED(pcvInitialize())) {
            stock_cpu_clock = getClock(Module::Cpu);
            stock_gpu_clock = getClock(Module::Gpu);
            stock_emc_clock = getClock(Module::Emc);
            initialized = true;
            brls::Logger::info("SwitchSys: Initialized (pcv) - CPU: {} MHz, GPU: {} MHz, EMC: {} MHz",
                stock_cpu_clock / 1000000, stock_gpu_clock / 1000000, stock_emc_clock / 1000000);
        }
    } else {
        if (R_SUCCEEDED(clkrstInitialize())) {
            stock_cpu_clock = getClock(Module::Cpu);
            stock_gpu_clock = getClock(Module::Gpu);
            stock_emc_clock = getClock(Module::Emc);
            initialized = true;
            brls::Logger::info("SwitchSys: Initialized (clkrst) - CPU: {} MHz, GPU: {} MHz, EMC: {} MHz",
                stock_cpu_clock / 1000000, stock_gpu_clock / 1000000, stock_emc_clock / 1000000);
        }
    }
}

void SwitchSys::exit() {
    if (!initialized) return;

    // Restore stock clocks
    setClock(false);

    bool is_old = hosversionBefore(8, 0, 0);
    if (is_old) {
        pcvExit();
    } else {
        clkrstExit();
    }

    initialized = false;
    brls::Logger::info("SwitchSys: Exited and restored stock clocks");
}

void SwitchSys::setClock(bool overclock) {
    if (!initialized) {
        brls::Logger::warning("SwitchSys: Not initialized, cannot set clock");
        return;
    }

    if (overclock) {
        setClock(Module::Cpu, (int)CPUClock::Max);
        setClock(Module::Gpu, (int)GPUClock::Max);
        setClock(Module::Emc, (int)EMCClock::Max);
        brls::Logger::info("SwitchSys: Overclock enabled - CPU: 1785 MHz, GPU: 921 MHz, EMC: 1600 MHz");
    } else if (getClock(Module::Cpu) != getClock(Module::Cpu, true)) {
        setClock(Module::Cpu, (int)CPUClock::Stock);
        setClock(Module::Gpu, (int)GPUClock::Stock);
        setClock(Module::Emc, (int)EMCClock::Stock);
        brls::Logger::info("SwitchSys: Restored stock clocks - CPU: {} MHz, GPU: {} MHz, EMC: {} MHz",
            stock_cpu_clock / 1000000, stock_gpu_clock / 1000000, stock_emc_clock / 1000000);
    }
}

int SwitchSys::getClock(const SwitchSys::Module& module, bool stockClocks) {
    u32 hz = 0;
    int bus_id = 0;
    bool is_old = hosversionBefore(8, 0, 0);

    if (stockClocks) {
        switch (module) {
        case SwitchSys::Module::Cpu:
            return stock_cpu_clock;
        case SwitchSys::Module::Gpu:
            return stock_gpu_clock;
        case SwitchSys::Module::Emc:
            return stock_emc_clock;
        default:
            return 0;
        }
    }

    switch (module) {
    case SwitchSys::Module::Cpu:
        bus_id = is_old ? (int)module : (int)PcvModuleId_CpuBus;
        break;
    case SwitchSys::Module::Gpu:
        bus_id = is_old ? (int)module : (int)PcvModuleId_GPU;
        break;
    case SwitchSys::Module::Emc:
        bus_id = is_old ? (int)module : (int)PcvModuleId_EMC;
        break;
    default:
        break;
    }

    if (is_old) {
        pcvGetClockRate((PcvModule)bus_id, &hz);
    } else {
        ClkrstSession session = {0};
        clkrstOpenSession(&session, (PcvModuleId)bus_id, 3);
        clkrstGetClockRate(&session, &hz);
        clkrstCloseSession(&session);
    }
    return (int)hz;
}

bool SwitchSys::setClock(const SwitchSys::Module& module, int hz) {
    int new_hz = hz;
    int bus_id = (int)module;
    bool is_old = hosversionBefore(8, 0, 0);

    switch (module) {
    case SwitchSys::Module::Cpu:
        new_hz = new_hz <= 0 ? stock_cpu_clock : new_hz;
        bus_id = is_old ? (int)module : (int)PcvModuleId_CpuBus;
        break;
    case SwitchSys::Module::Gpu:
        new_hz = new_hz <= 0 ? stock_gpu_clock : new_hz;
        bus_id = is_old ? (int)module : (int)PcvModuleId_GPU;
        break;
    case SwitchSys::Module::Emc:
        new_hz = new_hz <= 0 ? stock_emc_clock : new_hz;
        bus_id = is_old ? (int)module : (int)PcvModuleId_EMC;
        break;
    default:
        break;
    }

    if (is_old) {
        if (R_SUCCEEDED(pcvSetClockRate((PcvModule)bus_id, (u32)new_hz))) {
            return true;
        }
    } else {
        ClkrstSession session = {0};
        clkrstOpenSession(&session, (PcvModuleId)bus_id, 3);
        clkrstSetClockRate(&session, new_hz);
        clkrstCloseSession(&session);
        return true;
    }
    return false;
}

#endif
