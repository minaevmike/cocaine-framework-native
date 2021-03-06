#include "cocaine/framework/detail/log.hpp"
#include "cocaine/framework/trace.hpp"

#include "cocaine/framework/trace_logger.hpp"
#include "cocaine/framework/service.hpp"
#include "cocaine/framework/manager.hpp"

#include <cocaine/idl/logging.hpp>
#include <cocaine/traits/enum.hpp>
#include <cocaine/traits/vector.hpp>
#include <cocaine/traits/optional.hpp>
#include <cocaine/traits/attributes.hpp>
#include <cocaine/locked_ptr.hpp>

#include <asio/io_service.hpp>

#include <functional>
#include <thread>
namespace cocaine { namespace framework {
namespace {
    void
    run_asio(asio::io_service& loop) {
        CF_DBG("Starting loop...");
        loop.run();
        CF_DBG("Stopped loop");
    }

    class log_handler_t :
    public std::enable_shared_from_this<log_handler_t> {
    public:
        log_handler_t(std::shared_ptr<framework::service<io::log_tag>> _logger, std::string _message, blackhole::attribute::set_t _attributes) :
            logger(std::move(_logger)),
            message(std::move(_message)),
            attributes(std::move(_attributes))
        {
            attributes.push_back(blackhole::attribute::make("real_timestamp", current_time()));
        }

        void
        operator()() {
            CF_DBG("SENDING %s ", message.c_str());
            logger->invoke<io::log::emit>(
                logging::info,
                std::string("app/trace"),
                std::move(message),
                std::move(attributes)
            );
            CF_DBG("SENT %s ", message.c_str());
        }

        static
        uint64_t
        current_time() {
            return std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                   ).count();
        }

    private:
        std::shared_ptr<framework::service<io::log_tag>> logger;
        std::string message;
        blackhole::attribute::set_t attributes;
    };
}

class internal_logger_t::impl {
public:
    impl(std::shared_ptr<service<io::log_tag>> logger_service) :
        logger(std::move(logger_service)),
        loop(),
        work(boost::optional<asio::io_service::work>(asio::io_service::work(loop))),
        chamber(run_asio, std::ref(loop))
    {}

    ~impl() {
        work.reset();
        chamber.join();
    }

    std::shared_ptr<framework::service<io::log_tag>> logger;
    asio::io_service loop;
    boost::optional<asio::io_service::work> work;
    std::thread chamber;
};

void
internal_logger_t::log(std::string message) {
    if(!d) {
        return;
    }
    //std::bind here is intentional. We don't want to pass trace there.
    d->loop.post(std::bind(
        &log_handler_t::operator(),
        std::make_shared<log_handler_t>(d->logger, std::move(message), trace_t::current().attributes<blackhole::attribute::set_t>())
    ));
}

internal_logger_t::internal_logger_t(std::shared_ptr<service<io::log_tag>> logger_service) :
    d(new impl(std::move(logger_service)))
{
}

internal_logger_t::internal_logger_t():
    d(nullptr)
{}

internal_logger_t::internal_logger_t(internal_logger_t&& other) :
    d(std::move(other.d))
{
}

internal_logger_t::~internal_logger_t()
{}

}}
