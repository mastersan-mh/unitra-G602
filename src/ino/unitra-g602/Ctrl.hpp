#ifndef CTRL_H_INCLUDED_
#define CTRL_H_INCLUDED_

namespace app
{

//#define CTRL_DEBUG

#define CTRL_WARNING_SPEED_TOO_LOW   (1 << 0)
#define CTRL_WARNING_SPEED_TOO_HIGH  (1 << 1)

/**
 * @brief Class-controller
 */
class Ctrl
{
public:
    typedef int speed_t;

    enum class Event
    {
        ERRORS_UPDATE,
        WARNINGS_UPDATE,
        MOTOR_ON,
        MOTOR_OFF,
        MOTOR_SETPOINT_UPDATE,
        LIFT_UP,
        LIFT_DOWN,
    };

    typedef union
    {
        struct
        {
            unsigned errors;
        } ERRORS_UPDATE;
        struct
        {
            unsigned warnings;
        } WARNINGS_UPDATE;
        struct
        {
            speed_t setpoint;
        } DRIVE_SETPOINT_UPDATE;
    }  EventData;

    enum class RunMode
    {
        STOPPED,
        STARTED_AUTO,
        STARTED_MANUAL,
        SERVICE_MODE_1,
    };

    enum class BaselineSpeedMode
    {
        MODE_LOW,
        MODE_HIGH,
    };
#define CTRL_BASESPEEDMODE__NUM (static_cast<int>(BaselineSpeedMode::MODE_HIGH) + 1)

    Ctrl() = delete;
    /**
     * @param baseSpeedLow      Базовая скорость: низкая
     * Gparam baseSpeedHigh     Базовая скорость: высокая
     */
    Ctrl(
            speed_t baseSpeedLow,
            speed_t baseSpeedHigh,
            void (*eventFunc)(Event event, const EventData& data, void * args),
            void * args
    );
    Ctrl(const Ctrl&) = delete;
    Ctrl& operator=(const Ctrl&) = delete;

    ~Ctrl();
    /**
     * @brief Выбор базовой скорости
     */
    void baselineSpeedModeSet(BaselineSpeedMode baselineSpeedMode, void * args);
    /**
     * @brief Вручную задать отклонение скорости от выбраной базовой скорости
     */
    void manualSpeedSet(speed_t speed, void * args);
    void autostopAllowSet(bool allow_autostop, void * args);

    void start(void * args);
    void stop(void * args);
    /** @brief needle setting: drive stopped, lift down */
    void service_mode_1(void * args);

    /**
     * @param speed     Current table speed
     */
    void actualSpeedUpdate(speed_t speed, void * args);

    /**
     * @brief сработал датчик автостопа?
     */
    void stopTriggeredSet(bool triggered, void * args);

    RunMode runModeGet();

    int errorsGet() const;
    int warningsGet() const;

private:

    enum class Command
    {
        INIT,
        SPEED_BASELINE_LOW,
        SPEED_BASELINE_HIGH,
        SPEED_MANUAL_UPDATE,
        AUTOSTOP_ALLOW,
        AUTOSTOP_DENY,
        GAUGE_STOP_ON,
        GAUGE_STOP_OFF,
        START,
        STOP,
        SERVICE_MODE_1,
    };

    typedef union
    {
        struct
        {
            speed_t speed;
        } SPEED_MANUAL_UPDATE;
    } CommandData;

    enum class State
    {
        INIT,
        STOPPED,
        STARTED_AUTO,
        STARTED_MANUAL,
        SERVICE_MODE_1, /* needle setting: drive stopped, lift down. Can start only from STOPPED */
    };

    speed_t P_speed_baseline_get() const;

    void P_event(Event event, const EventData& data, void * args) const;
    void P_event_errors_set(unsigned err, void * args);
    void P_event_errors_clear(unsigned err, void * args);
    void P_event_warnings_set(unsigned warn, void * args);
    void P_event_warnings_clean(unsigned warn, void * args);
    void P_event_motor_on(void * args);
    void P_event_motor_off(void * args);
    void P_event_motor_setpoint_update(Ctrl::speed_t setpoint, void * args);
    void P_event_lift_up(void * args);
    void P_event_lift_down(void * args);

    void P_fsm(Command cmd, const CommandData & data, void * args);

    /* init vars */
    void (*m_eventFunc)(Event event, const EventData& data, void * args);
    speed_t m_speed_baselines[CTRL_BASESPEEDMODE__NUM];

    State m_state;        /**< Finite State Machine state */
    unsigned m_state_errors; /**< bitmap of errors */
    unsigned m_state_warnings; /**< bitmap of warnings */
    bool m_state_allowed_autostop;
    bool m_state_autostop_triggered;

    /* user control variables */
    speed_t m_speed_manual_delta;

    BaselineSpeedMode m_speed_baseline_mode;

    CommandData m_cmdData;
    EventData m_eventData;

#ifdef CTRL_DEBUG
public:
    typedef struct
    {
        State m_state;
        speed_t m_speed_manual_delta;
    } internal_state_t;
    void debug_get(internal_state_t * state) const;
#endif

};

} /* namespace app */
#endif /* CTRL_H_INCLUDED_ */
