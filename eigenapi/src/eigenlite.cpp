#include <eigenapi.h>
#include "eigenlite_impl.h"
#include <picross/pic_time.h>
#include <picross/pic_log.h>
#include <string.h>

#define VERSION_STRING "eigenlite v0.6 Alpha/Tau/Pico, experimental - Author: TheTechnobear"

namespace EigenApi {
    void EigenLite::logmsg(const char *msg) {
        pic::logmsg() << msg;
    }

    // public interface
    EigenLite::EigenLite(IFW_Reader &fwReader) : fwReader(fwReader), pollTime_(100) {
    }

    EigenLite::~EigenLite() {
        destroy();
    }

    void EigenLite::addCallback(EigenApi::Callback *api) {
        // do not allow callback to be added twice
        for (auto cb: callbacks_) {
            if (cb == api) {
                return;
            }
        }
        callbacks_.push_back(api);
    }

    void EigenLite::removeCallback(EigenApi::Callback *api) {
        std::vector<Callback *>::iterator iter;
        for (iter = callbacks_.begin(); iter != callbacks_.end(); iter++) {
            if (*iter == api) {
                callbacks_.erase(iter);
                return;
            }
        }
    }

    void EigenLite::clearCallbacks() {
        std::vector<Callback *>::iterator iter;
        while (!callbacks_.empty()) {
            callbacks_.pop_back();
        }
    }

    volatile bool discoverProcessRun = true;

    void *discoverProcess(void *pthis) {
        auto pThis = static_cast<EigenLite *>(pthis);
        std::string usbdev;
        while (discoverProcessRun) {
            pThis->checkUsbDev();
            pic_microsleep(10000000);
        }
        return nullptr;
    }

    void EigenLite::checkUsbDev() {
        if (!usbDevChange_) {
            baseUSBDev_ = EF_BaseStation::availableDevice();
            picoUSBDev_ = EF_Pico::availableDevice();
            usbDevChange_ = baseUSBDev_.size() > 0 || picoUSBDev_.size() > 0;
        }
    }

    bool EigenLite::create() {
        logmsg(VERSION_STRING);
        logmsg("start EigenLite");
        pic_init_time();
        discoverProcessRun = true;
        discoverThread_ = std::thread(discoverProcess, this);
        pic_set_foreground(true);
        lastPollTime_ = 0;
        usbDevChange_ = false;
        return true;
    }

    bool EigenLite::destroy() {
        discoverProcessRun = false;
        if (discoverThread_.joinable()) {
            try {
                discoverThread_.join();
            } catch (std::system_error &) {
                logmsg("warn error whilst joining to discover thread");
            }

        }
        for (auto device: devices_) {
            device->destroy();
        }
        devices_.clear();
        return true;
    }

    void EigenLite::deviceDead(const char *dev, unsigned reason) {
        deadDevices_.insert(dev);
    }


    bool EigenLite::poll() {
        if (usbDevChange_) {
            bool newPico = picoUSBDev_.size() > 0;
            bool newBase = baseUSBDev_.size() > 0;;

            for (auto dev: devices_) {
                if (picoUSBDev_ == dev->usbDevice()->name()) {
                    newPico = false;
                }
                if (baseUSBDev_ == dev->usbDevice()->name()) {
                    newBase = false;
                }
            }

            EF_Harp *pDevice = nullptr;
            if (newPico) {
                char logbuf[100];
                snprintf(logbuf, 100, "new pico %s", picoUSBDev_.c_str());
                logmsg(logbuf);

                pDevice = new EF_Pico(*this);
                if (pDevice->create()) {
                    char logbuf[100];
                    snprintf(logbuf, 100, "created pico %s", pDevice->usbDevice()->name());
                    logmsg(logbuf);
                    devices_.push_back(pDevice);
                    pDevice->start();
                }
            }
            if (newBase) {
                char logbuf[100];
                snprintf(logbuf, 100, "new base %s", baseUSBDev_.c_str());
                logmsg(logbuf);

                pDevice = new EF_BaseStation(*this);
                if (pDevice->create()) {
                    devices_.push_back(pDevice);
                    pDevice->start();
                }
            }

            usbDevChange_ = false;
            if (newBase || newPico) return true;
        }

        while (deadDevices_.size() > 0) {
            std::string usbname = *deadDevices_.begin();

            std::vector<EF_Harp *>::iterator iter;
            bool found = false;
            for (iter = devices_.begin(); !found && iter != devices_.end(); iter++) {
                EF_Harp *pDevice = *iter;
                if (pDevice->name() == usbname) {
                    char logbuf[100];
                    snprintf(logbuf, 100, "destroy device %s", usbname.c_str());
                    logmsg(logbuf);
                    pDevice->destroy();
                    found = true;
                    devices_.erase(iter);
                }
            }
            deadDevices_.erase(usbname);
        }

        long long t = pic_microtime();
        long long diff = t - lastPollTime_;

        if (diff > pollTime_) {
            lastPollTime_ = t;
            bool ret = true;
            for (auto pDevice: devices_) {
                ret &= pDevice->poll(0);
            }
            return ret;
        }
        return false;

    }

    void EigenLite::setPollTime(unsigned pollTime) {
        pollTime_ = pollTime;
    }


    void EigenLite::fireDeviceEvent(const char *dev,
                                    Callback::DeviceType dt, int rows, int cols, int ribbons, int pedals) {
        for (auto cb: callbacks_) {
            cb->device(dev, dt, rows, cols, ribbons, pedals);
        }
    }

    void EigenLite::fireDisconnectEvent(const char *dev, Callback::DeviceType dt) {
        for (auto cb: callbacks_) {
            cb->disconnect(dev, dt);
        }
    }

    void
    EigenLite::fireKeyEvent(const char *dev, unsigned long long t, unsigned course, unsigned key, bool a, unsigned p,
                            int r, int y) {
        for (auto cb: callbacks_) {
            cb->key(dev, t, course, key, a, p, r, y);
        }
    }

    void EigenLite::fireBreathEvent(const char *dev, unsigned long long t, unsigned val) {
        for (auto cb: callbacks_) {
            cb->breath(dev, t, val);
        }
    }

    void EigenLite::fireStripEvent(const char *dev, unsigned long long t, unsigned strip, unsigned val, bool a) {
        for (auto cb: callbacks_) {
            cb->strip(dev, t, strip, val, a);
        }
    }

    void EigenLite::firePedalEvent(const char *dev, unsigned long long t, unsigned pedal, unsigned val) {
        for (auto cb: callbacks_) {
            cb->pedal(dev, t, pedal, val);
        }
    }

    void EigenLite::fireDeadEvent(const char *dev, unsigned reason) {
        deviceDead(dev, reason);
        for (auto cb: callbacks_) {
            cb->dead(dev, reason);
        }
    }

    void EigenLite::setLED(const char *dev, unsigned course, unsigned int key, unsigned int colour) {
        for (auto pDevice: devices_) {
            if (dev == NULL || dev == pDevice->name() || strcmp(dev, pDevice->name()) == 0) {
                pDevice->setLED(course, key, colour);
            }
        }
    }
} // namespace EigenApi

