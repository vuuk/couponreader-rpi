#ifndef SERIALCOMHANDLER_H
#define SERIALCOMHANDLER_H
#include <string>
#include <thread>
#include <map>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/system/error_code.hpp>
#include <vector>
#include <QDebug>
#include <regex>
#include <iostream>
#include <initializer_list>

#define BUFF_SIZE 64

template <typename T>
class SerialcomHandler
{
    using serial_port = boost::asio::serial_port;
    using io_service = boost::asio::io_service;
    using command_type = std::pair<const std::string, const unsigned char>;
    using command_map = std::map<const std::string, const unsigned char>;
    using command_pair = std::pair<const std::string, const std::string>;
    using spsc_queue = boost::lockfree::spsc_queue<T, boost::lockfree::capacity<1024> >;
    using size_type = typename spsc_queue::size_type;

    using port_error = boost::system::error_code;
    enum PAIR_COMMANDS
    {
        START = 0,
        STOP
    };
    enum SLEEP{
        SHORT = 50,
        NORMAL = 100,
        LONG = 500
    };

public:
    ~SerialcomHandler();

    static SerialcomHandler& instance() { static SerialcomHandler<T> singleton; return singleton; }
    void open ();
    void close ();
    bool is_open () const {return port_.is_open(); }

    void setPortName (const std::string &s) { port_name_ = s; }
    void setRegexPattern (const std::string& s) { regex_pattern_ = s; }

    bool is_reading() const { return reading_flag_; }

    void add_commands (command_type &c) { commands_.insert(c); }
    void add_commands (std::initializer_list<command_type> il) { commands_.insert(il.begin(), il.end()); }

    void negate_read_flag () { reading_flag_ = !reading_flag_; }

    void write_commands (const std::initializer_list<const std::string>, std::size_t sleep_ms = SLEEP::LONG, bool close_when_done = true);

    size_type available_reads () const { return readings_.read_available(); }

    void start_sensors(command_pair c);
    void stop_sensors();
    bool valid_port() const { return !port_name_.empty(); }
    bool valid_regex_pattern() const { return !regex_pattern_.empty(); }
    bool check_port();
    T read_available();

protected:
     SerialcomHandler();
     void start_sensor_readings (const std::string, const std::string);
     bool write_command (const std::string command, std::size_t sleep_ms = SLEEP::NORMAL);
     void reset_port();
private:
    spsc_queue          readings_;
    io_service          io_;
    serial_port         port_;
    std::string         port_name_;
    std::thread         producer_thread_;

    std::string   regex_pattern_;
    command_map         commands_;
    bool                reading_flag_ = false;
};
template <typename T>
SerialcomHandler<T>::SerialcomHandler() :
    readings_(), io_(), port_(io_), reading_flag_(false)
    { }

template <typename T>
SerialcomHandler<T>::~SerialcomHandler()
{
    close();
    if (producer_thread_.joinable())
        producer_thread_.join();
}

template <typename T>
void SerialcomHandler<T>::open()
{
    if (!is_open())
        port_.open(port_name_);
}

template <typename T>
void SerialcomHandler<T>::close()
{
    if (is_open())
        port_.close();
}
template <typename T>
bool SerialcomHandler<T>::write_command (const std::string command, std::size_t sleep_ms)
{
    auto result = commands_.at(command);
    if (is_open()) {
        try
        {
            boost::asio::write( port_, boost::asio::buffer(&result, 1) );
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            return true;
        }
        catch (const boost::system::system_error &e)
        {
            std::cerr << e.what() << std::endl;
            return false;;
        }
    }
    return false;
}

template <typename T>
void SerialcomHandler<T>::write_commands (const std::initializer_list<const std::string> command_list, std::size_t sleep_ms, bool close_when_done)
{
    open();
    for (auto &s : command_list)
        if (!write_command(s, sleep_ms))
            return;
    if (close_when_done)
        close();
}

template <typename T>
void SerialcomHandler<T>::start_sensors(command_pair c)
{
    reading_flag_ = true;
    producer_thread_ = std::thread(&SerialcomHandler<T>::start_sensor_readings, this, c.first, c.second);
}

template <typename T>
void SerialcomHandler<T>::stop_sensors()
{
    reading_flag_ = false;
    if (producer_thread_.joinable()) {
        producer_thread_.join();
    }
}

template <typename T>
void SerialcomHandler<T>::start_sensor_readings (const std::string start, const std::string stop)
{
    try {
    reset_port();

    write_commands({start}, SLEEP::NORMAL, false);
    std::regex r(regex_pattern_);

    while ( reading_flag_ && is_open() && readings_.write_available()) {
        unsigned char read_buff[BUFF_SIZE];
        read_buff[0] = '\0';

        auto bytes_read = boost::asio::read(port_, boost::asio::buffer(&read_buff, BUFF_SIZE));

        std::string buff_str(read_buff, read_buff + bytes_read);

        for ( std::sregex_iterator beg_it(buff_str.begin(), buff_str.end(), r), end_it;
              beg_it != end_it; ++beg_it)
        {
            readings_.push(T(beg_it->str()));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP::NORMAL));
    }
    write_commands({stop}, SLEEP::SHORT, true);
    }
    catch (const boost::system::system_error &e)
    {
        reset_port();
        std::cerr << e.what() << std::endl;
        return;
    }
}

template <typename T>
bool SerialcomHandler<T>::check_port()
{
   port_error ret;
   if (!is_open())
        port_.open(port_name_, ret);
   return !ret;
}

template <typename T>
T SerialcomHandler<T>::read_available()
{
    if (available_reads()) {
       T ret;
       readings_.pop(ret);
       return ret;
    }
    return T();
}
template <typename T>
void SerialcomHandler<T>::reset_port()
{
    open();
    close();
}
#endif // SERIALCOMHANDLER_H
