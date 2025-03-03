#include <eigenapi.h>
#include "eigenlite_impl.h"
#include <picross/pic_log.h>

namespace EigenApi
{
    Eigenharp::Eigenharp(IFW_Reader &fwReader)
    {
        impl=new EigenLite(fwReader);
    }

    Eigenharp::~Eigenharp()
    {
        delete static_cast<EigenLite*>(impl);
    }

    bool Eigenharp::start()
    {
        return static_cast<EigenLite*>(impl)->create();
    }
    
    bool Eigenharp::stop()
    {
        return static_cast<EigenLite*>(impl)->destroy();
    }
    
    bool Eigenharp::process()
    {
        return static_cast<EigenLite*>(impl)->poll();
    }
    
    void Eigenharp::addCallback(Callback* api)
    {
        static_cast<EigenLite*>(impl)->addCallback(api);
    }
        
    void Eigenharp::removeCallback(Callback* api)
    {
        static_cast<EigenLite*>(impl)->removeCallback(api);
    }
        
    void Eigenharp::clearCallbacks()
    {
        static_cast<EigenLite*>(impl)->clearCallbacks();
    }

    void Eigenharp::setPollTime(unsigned pollTime)
    {
        static_cast<EigenLite*>(impl)->setPollTime(pollTime);
    }

    void Eigenharp::setLED(const char* dev, unsigned course, unsigned int key,unsigned int colour)
    {
        static_cast<EigenLite*>(impl)->setLED(dev,course, key, colour);
    }
    
    // basic logger, if its not overriden
    class logger : public pic::logger_t
    {
    public:
        void log(const char * x)
        {
            Logger::logmsg(x);
        }
    };
    
    static logger theLogger;
    
    /// logging interface
    void (*Logger::_logmsg)(const char*);
    void Logger::setLogFunc(void (*pLogFn)(const char*))
    {
        Logger::_logmsg=pLogFn;
        pic::logger_t::tsd_setlogger(&theLogger);
    }
    void Logger::logmsg(const char* msg)
    {
        if(Logger::_logmsg!=NULL) Logger::_logmsg(msg);
    }
} // namespace eigenapi
