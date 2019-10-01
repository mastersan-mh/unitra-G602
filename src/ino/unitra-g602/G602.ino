/**
 * @file G602.cpp
 */

#include "G602.hpp"

#define TO_BOOL(x) ((x) != 0)

G602::G602(
    int baselineSpeedLow,
    int baselineSpeedHigh,
    void (*event_strober)(bool on),
    void (*event_lift_up)(),
    void (*event_lift_down)(),
    void (*event_motor_update)(bool state, int setpoint)
)
: m_event_strober(event_strober)
, m_event_lift_up(event_lift_up)
, m_event_lift_down(event_lift_down)
, m_event_motor_update(event_motor_update)
, m_time_now(0)
, m_time_next(0)
, m_service_mode(0)
, m_motor_on(false)
, m_motor_setpoint(0)
, sched()
, m_blinker()
, m_ctrl(baselineSpeedLow, baselineSpeedHigh, P_ctrl_event, this)
, m_di_gauge_stop(false, P_event_stopUnset, P_event_stopSet, this, DI_DEBOUNCE_TIME)
, m_di_btn_speed_mode(false, P_event_speedMode33,  P_event_speedMode45, this, DI_DEBOUNCE_TIME)
, m_di_btn_autostop(false, P_event_autostopEnable,  P_event_autostopDisable, this, DI_DEBOUNCE_TIME)
, m_di_btn_start(false, P_event_start,  nullptr, this, DI_DEBOUNCE_TIME)
, m_di_btn_stop(false, P_event_stop,  P_event_stop_release, this, DI_DEBOUNCE_TIME)
{
}

G602::~G602()
{
}

void G602::timeSet(unsigned long time_now)
{
    m_time_now = time_now;
}

void G602::loop()
{
    if(m_time_now < m_time_next)
    {
        return;
    }

/*    long late = (long)time - (long)time_next; */
    sched.handle(m_time_now);
    m_time_next = P_rtcNextTimeGet();

#ifdef CTRL_DEBUG
    app::Ctrl::internal_state_t state;
    g602.m_ctrl.debug_get(&state);
    DEBUG_PRINT("m_state = "); DEBUG_PRINT((int)state.m_state);
    DEBUG_PRINT("; m_speed_manual_delta = "); DEBUG_PRINT((int)state.m_speed_manual_delta);
    DEBUG_PRINTLN();
#endif

}

void G602::notifyGaugeStopSet(bool state)
{
    m_di_gauge_stop.stateSet(state, this, m_time_now);
}

void G602::notifyButtonSpeedModeSet(bool state)
{
    m_di_btn_speed_mode.stateSet(state, this, m_time_now);
}

void G602::notifyButtonAutostopSet(bool state)
{
    m_di_btn_autostop.stateSet(state, this, m_time_now);
}

void G602::notifyButtonStartSet(bool state)
{
    m_di_btn_start.stateSet(state, this, m_time_now);
}

void G602::notifyButtonStopSet(bool state)
{
    m_di_btn_stop.stateSet(state, this, m_time_now);
}

void G602::manualSpeedSet(int speed)
{
    m_ctrl.manualSpeedSet(speed, this);
}

void G602::actualSpeedUpdate(int speed)
{
    m_ctrl.actualSpeedUpdate(speed, this);
}

unsigned long G602::P_rtcNextTimeGet() const
{
    unsigned long time_next = sched.nearestTime();
    return (time_next > 0 ? time_next : m_time_now + 10 );
}

void G602::P_blinker_start(GBlinker::BlinkType type)
{
    unsigned actions = m_blinker.typeSet(type, true);

    if(actions & GBLINKER_ACTIONFLAG_UNSCHEDULE)
    {
        sched.unshedule(shed_task_id_blinker);
    }
    if(actions & GBLINKER_ACTIONFLAG_SCHEDULE)
    {
        sched.shedule(shed_task_id_blinker, m_time_now, P_task_blinker, this);
        m_time_next = P_rtcNextTimeGet();
    }
}

void G602::P_blinker_stop(GBlinker::BlinkType type)
{
    unsigned actions = m_blinker.typeSet(type, false);

    if(actions & GBLINKER_ACTIONFLAG_UNSCHEDULE)
    {
        sched.unshedule(shed_task_id_blinker);
    }
    if(actions & GBLINKER_ACTIONFLAG_SCHEDULE)
    {
        sched.shedule(shed_task_id_blinker, m_time_now, P_task_blinker, this);
        m_time_next = P_rtcNextTimeGet();
    }
}

void G602::P_task_blinker(nostd::size_t id, unsigned long time, unsigned long now, G602Scheduler & sched, void * args)
{
    G602_DEFINE_SELF();
    bool end = false;
    bool light = false;
    unsigned long wait_time = 0;
    self->m_blinker.scheduledPartGet(&end, &light, &wait_time);
    if(end)
    {
        self->m_event_strober(true);
    }
    else
    {
        self->m_event_strober(!light);
        sched.shedule(id, time + wait_time, G602::P_task_blinker, args);
    }
}

void G602::P_task_awaiting_service_mode(nostd::size_t id, unsigned long time, unsigned long now, G602Scheduler & sched, void * args)
{
    G602_DEFINE_SELF();
    self->m_service_mode = 1;

    self->P_blinker_start(GBlinker::BlinkType::ON_ENTER_SERVICE_MODE);
}

void G602::P_motor_update()
{
    m_event_motor_update(m_motor_on, m_motor_setpoint);
}

void G602::P_ctrl_event(app::Ctrl::Event event, const app::Ctrl::EventData& data, void * args)
{
    G602_DEFINE_SELF();
    switch(event)
    {
        case app::Ctrl::Event::ERRORS_UPDATE:
        {
            break;
        }

        case app::Ctrl::Event::WARNINGS_UPDATE:
        {
            bool blink_speed_to_low  = TO_BOOL(data.WARNINGS_UPDATE.warnings & CTRL_WARNING_SPEED_TOO_LOW);
            bool blink_speed_to_high = TO_BOOL(data.WARNINGS_UPDATE.warnings & CTRL_WARNING_SPEED_TOO_HIGH);

            if(blink_speed_to_low)
            {
                self->P_blinker_start(GBlinker::BlinkType::ON_TOO_LOW_SPEED);
            }
            else
            {
                self->P_blinker_stop(GBlinker::BlinkType::ON_TOO_LOW_SPEED);
            }

            if(blink_speed_to_high)
            {
                self->P_blinker_start(GBlinker::BlinkType::ON_TOO_HIGH_SPEED);
            }
            else
            {
                self->P_blinker_stop(GBlinker::BlinkType::ON_TOO_HIGH_SPEED);
            }

            break;
        }

        case app::Ctrl::Event::MOTOR_ON:
        {
            self->m_motor_on = true;
            self->P_motor_update();
            self->P_measures_start();
            break;
        }

        case app::Ctrl::Event::MOTOR_OFF:
        {
            self->m_motor_on = false;
            self->P_motor_update();
            self->P_measures_stop();
            break;
        }

        case app::Ctrl::Event::MOTOR_SETPOINT_UPDATE:
        {
            self->m_motor_setpoint = data.DRIVE_SETPOINT_UPDATE.setpoint;
            self->P_motor_update();
            break;
        }

        case app::Ctrl::Event::LIFT_UP:
        {
            self->m_event_lift_up();
            break;
        }

        case app::Ctrl::Event::LIFT_DOWN:
        {
            self->m_event_lift_down();
            break;
        }

    }

}

void G602::P_event_stopSet(void * args)
{
    G602_DEFINE_SELF();
    self->m_ctrl.stopTriggeredSet(true, self);
}

void G602::P_event_stopUnset(void * args)
{
    G602_DEFINE_SELF();
    self->m_ctrl.stopTriggeredSet(false, self);
}

void G602::P_event_speedMode45(void * args)
{
    G602_DEFINE_SELF();
    self->m_ctrl.baselineSpeedModeSet(app::Ctrl::BaselineSpeedMode::MODE_HIGH, self);
}

void G602::P_event_speedMode33(void * args)
{
    G602_DEFINE_SELF();
    self->m_ctrl.baselineSpeedModeSet(app::Ctrl::BaselineSpeedMode::MODE_LOW, self);
}

void G602::P_event_autostopEnable(void * args)
{
    G602_DEFINE_SELF();
    self->m_ctrl.autostopAllowSet(true, self);
}

void G602::P_event_autostopDisable(void * args)
{
    G602_DEFINE_SELF();
    self->m_ctrl.autostopAllowSet(false, self);
}

void G602::P_event_start(void * args)
{
    G602_DEFINE_SELF();
    if(self->m_service_mode > 0)
    {
        /* enter service mode #1 */
        self->m_ctrl.service_mode_1(self);
    }
    else
    {
        app::Ctrl::RunMode mode_prev = self->m_ctrl.runModeGet();
        self->m_ctrl.start(self);
        app::Ctrl::RunMode mode = self->m_ctrl.runModeGet();
        if(mode != mode_prev)
        {
            if(
                    mode_prev == app::Ctrl::RunMode::STARTED_AUTO ||
                    mode_prev == app::Ctrl::RunMode::STARTED_MANUAL
            )
            {
                switch(mode)
                {
                    case app::Ctrl::RunMode::STARTED_AUTO  : self->P_blinker_start(GBlinker::BlinkType::ON_AUTO); break;
                    case app::Ctrl::RunMode::STARTED_MANUAL: self->P_blinker_start(GBlinker::BlinkType::ON_MANUAL); break;
                    default: break;
                }
            }
            else
            {
                self->P_blinker_stop(GBlinker::BlinkType::ON_STOP);
                self->P_blinker_start(GBlinker::BlinkType::ON_START);
            }
        }
    }
}

void G602::P_event_stop(void * args)
{
    G602_DEFINE_SELF();

    if(self->m_service_mode > 0)
    {
        self->m_ctrl.stop(self);
        self->P_blinker_start(GBlinker::BlinkType::ON_STOP);
        self->m_service_mode = 0;
    }
    else
    {
        app::Ctrl::RunMode mode_prev = self->m_ctrl.runModeGet();
        self->m_ctrl.stop(self);
        app::Ctrl::RunMode mode = self->m_ctrl.runModeGet();
        if(mode != mode_prev)
        {
            self->P_blinker_stop(GBlinker::BlinkType::ON_START);
            self->P_blinker_start(GBlinker::BlinkType::ON_STOP);
        }
        self->sched.shedule(
            shed_task_id_service_mode_awaiting,
            self->m_time_now + service_mode_awaiting_tine,
            P_task_awaiting_service_mode,
            self
        );
    }
}

void G602::P_event_stop_release(void * args)
{
    G602_DEFINE_SELF();
    self->sched.unshedule(shed_task_id_service_mode_awaiting);
}
