// thread_prio_win.cpp - DSP-THREAD b (2026-10-01): an emulation worker thread as Windows treats an audio thread.
// Tamas measured: with the plugin's DSP thread, total CPU rose and jumped further when the host went to the
// background. Windows 11 throttles the threads of a background process (EcoQoS: lower clock / efficiency cores)
// unless they opt out; the host's own audio thread is registered with MMCSS, a plugin's own thread is not. So the
// worker (1) joins MMCSS "Pro Audio" and (2) opts out of execution-speed power throttling.
#ifdef _WIN32
#define NOMINMAX
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00   // SetThreadInformation (Windows 8+)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <avrt.h>
#pragma comment(lib, "avrt.lib")

namespace MS2000 {
void ms2kAudioWorkerThread()
{
    DWORD idx = 0;
    AvSetMmThreadCharacteristicsW(L"Pro Audio", &idx);
    THREAD_POWER_THROTTLING_STATE st{};
    st.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
    st.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
    st.StateMask = 0;                                    // throttling OFF for this thread
    SetThreadInformation(GetCurrentThread(), ThreadPowerThrottling, &st, sizeof st);
}
}
#elif defined(__APPLE__)
// MAC-2: the macOS counterpart - QoS USER_INTERACTIVE, so the scheduler keeps the thread on the performance
// cores and does not throttle it when the app goes to the background.
#include <pthread.h>
#include <pthread/qos.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
namespace MS2000 { void ms2kAudioWorkerThread()
{
    if (__builtin_available(macOS 10.10, *)) {
        pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
        return;
    }
    // MAC-LEGACY: OS X 10.9 has no QoS classes, so the worker would run at default timeshare priority - level
    // with the host's GUI threads - while the host's realtime audio thread waits for it every block: crackles
    // in Cubase 7.5 although the machine is fast enough (1.48x in the render test). Instead it gets the Mach
    // time-constraint policy CoreAudio's own IO threads use, with dsp56kBase's defaults for an unknown block
    // timing (no fixed period, computation 23 ms within 46 ms, preemptible).
    mach_timebase_info_data_t tb{};
    mach_timebase_info(&tb);
    const double usecToAbs = 1000.0 * double(tb.denom) / double(tb.numer);
    thread_time_constraint_policy_data_t pol;
    pol.period      = 0;
    pol.computation = uint32_t(23220.0 * usecToAbs);
    pol.constraint  = uint32_t(46440.0 * usecToAbs);
    pol.preemptible = TRUE;
    thread_policy_set(pthread_mach_thread_np(pthread_self()), THREAD_TIME_CONSTRAINT_POLICY,
                      reinterpret_cast<thread_policy_t>(&pol), THREAD_TIME_CONSTRAINT_POLICY_COUNT);
} }
#else
namespace MS2000 { void ms2kAudioWorkerThread() {} }
#endif
