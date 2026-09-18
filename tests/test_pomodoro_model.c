#include "pomodoro_model.h"

#include <assert.h>
#include <stdio.h>

static void test_defaults_and_selection(void) {
    pomodoro_model_t model;
    pomodoro_model_defaults(&model);
    assert(model.state == POMODORO_IDLE);
    assert(pomodoro_model_focus_min(&model) == 25);
    assert(model.remaining_sec == 25 * 60);
    assert(pomodoro_model_select_duration(&model, 1));
    assert(pomodoro_model_focus_min(&model) == 45);
    assert(pomodoro_model_select_duration(&model, 1));
    assert(pomodoro_model_focus_min(&model) == 15);
}

static void test_focus_pause_resume_and_completion(void) {
    pomodoro_model_t model;
    pomodoro_model_defaults(&model);
    assert(pomodoro_model_start_focus(&model, 1000));
    assert(model.session_id == 1);
    assert(pomodoro_model_pause(&model, 11000));
    assert(model.remaining_sec == 1490);
    assert(pomodoro_model_resume(&model, 20000));
    assert(pomodoro_model_tick(&model, 1509999) == POMODORO_EVENT_NONE);
    assert(pomodoro_model_tick(&model, 1510000) == POMODORO_EVENT_FOCUS_COMPLETE);
    assert(model.completed_sessions == 1);
    assert(model.completed_focus_min == 25);
    assert(model.state == POMODORO_REWARD);
    assert(model.pomodoro_round == 0);
    assert(model.pending_break_min == 5);
    assert(pomodoro_model_tick(&model, 1513000) == POMODORO_EVENT_REWARD_FINISHED);
    assert(model.state == POMODORO_BREAK_PROMPT);
    assert(model.pomodoro_round == 0);
    assert(pomodoro_model_tick(&model, 1514000) == POMODORO_EVENT_NONE);
    assert(model.completed_sessions == 1);
}

static void test_focus_pause_at_deadline_keeps_completion_event(void) {
    pomodoro_model_t model;
    pomodoro_model_defaults(&model);
    assert(pomodoro_model_start_focus(&model, 1000));

    uint64_t deadline = 1000 + pomodoro_model_focus_min(&model) * 60000ULL;
    assert(!pomodoro_model_pause(&model, deadline));
    assert(model.state == POMODORO_FOCUS_RUNNING);
    assert(model.remaining_sec == 0);
    assert(pomodoro_model_tick(&model, deadline) == POMODORO_EVENT_FOCUS_COMPLETE);
    assert(model.state == POMODORO_REWARD);
    assert(model.completed_sessions == 1);
}

static void test_abandon_timeout_and_confirm(void) {
    pomodoro_model_t model;
    pomodoro_model_defaults(&model);
    pomodoro_model_start_focus(&model, 0);
    pomodoro_model_pause(&model, 1000);
    assert(pomodoro_model_request_abandon(&model, 2000));
    assert(pomodoro_model_tick(&model, 6999) == POMODORO_EVENT_NONE);
    assert(pomodoro_model_tick(&model, 7000) == POMODORO_EVENT_CONFIRM_TIMEOUT);
    assert(model.state == POMODORO_FOCUS_PAUSED);
    assert(pomodoro_model_request_abandon(&model, 8000));
    assert(pomodoro_model_confirm_abandon(&model));
    assert(model.state == POMODORO_IDLE);
}

static void complete_focus(pomodoro_model_t *model, uint64_t now_ms) {
    assert(pomodoro_model_start_focus(model, now_ms));
    uint64_t end_ms = now_ms + pomodoro_model_focus_min(model) * 60000ULL;
    assert(pomodoro_model_tick(model, end_ms) == POMODORO_EVENT_FOCUS_COMPLETE);
    assert(pomodoro_model_tick(model, end_ms + POMODORO_REWARD_MS) ==
           POMODORO_EVENT_REWARD_FINISHED);
}

static void test_break_cycle_and_growth(void) {
    pomodoro_model_t model;
    pomodoro_model_defaults(&model);
    uint64_t now_ms = 0;
    for (int i = 0; i < 4; i++) {
        complete_focus(&model, now_ms);
        assert(model.pending_break_min == (i == 3 ? 15 : 5));
        /* Round stays with the completed focus until break ends or is skipped. */
        assert(model.pomodoro_round == (uint8_t)i);
        assert(pomodoro_model_skip_break(&model));
        assert(model.pomodoro_round == (uint8_t)((i + 1) % 4));
        now_ms += 2000000;
    }
    assert(model.pomodoro_round == 0);
    assert(model.completed_sessions == 4);
    assert(pomodoro_model_cat_stage(&model) == 1);
    uint32_t current, target;
    pomodoro_model_growth(&model, &current, &target);
    assert(current == 1 && target == 5);

    static const uint32_t sessions[] = { 0, 2, 3, 7, 8, 14, 15, 24, 25, 30 };
    static const uint8_t stages[] =   { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4 };
    for (size_t i = 0; i < sizeof(sessions) / sizeof(sessions[0]); i++) {
        model.completed_sessions = sessions[i];
        assert(pomodoro_model_cat_stage(&model) == stages[i]);
    }
}

static void test_break_timer(void) {
    pomodoro_model_t model;
    pomodoro_model_defaults(&model);
    model.state = POMODORO_BREAK_PROMPT;
    model.pending_break_min = 5;
    assert(pomodoro_model_start_break(&model, 1000));
    assert(pomodoro_model_tick(&model, 300999) == POMODORO_EVENT_NONE);
    assert(pomodoro_model_tick(&model, 301000) == POMODORO_EVENT_BREAK_COMPLETE);
    assert(model.state == POMODORO_IDLE);
    assert(model.pomodoro_round == 1);
}

static void test_break_pause_at_deadline_keeps_completion_event(void) {
    pomodoro_model_t model;
    pomodoro_model_defaults(&model);
    model.state = POMODORO_BREAK_PROMPT;
    model.pending_break_min = 5;
    assert(pomodoro_model_start_break(&model, 1000));

    uint64_t deadline = 1000 + 5 * 60000ULL;
    assert(!pomodoro_model_pause(&model, deadline));
    assert(model.state == POMODORO_BREAK_RUNNING);
    assert(model.break_remaining_sec == 0);
    assert(pomodoro_model_tick(&model, deadline) == POMODORO_EVENT_BREAK_COMPLETE);
    assert(model.state == POMODORO_IDLE);
    assert(model.pomodoro_round == 1);
}

static void test_restore_is_safe(void) {
    pomodoro_model_t model;
    pomodoro_model_defaults(&model);
    model.state = POMODORO_FOCUS_RUNNING;
    model.remaining_sec = 321;
    model.deadline_ms = 999999;
    pomodoro_model_restore(&model);
    assert(model.state == POMODORO_FOCUS_PAUSED);
    assert(model.remaining_sec == 321);
    assert(model.deadline_ms == 0);

    model.state = POMODORO_IDLE;
    model.reward_pending = true;
    pomodoro_model_restore(&model);
    assert(model.state == POMODORO_REWARD);
}


static void test_round_stays_through_reward_break_then_advances(void) {
    pomodoro_model_t model;
    pomodoro_model_defaults(&model);
    uint64_t now_ms = 0;

    /* Focus 1: round stays 0 through reward/break prompt; skip advances to 1. */
    complete_focus(&model, now_ms);
    assert(model.state == POMODORO_BREAK_PROMPT);
    assert(model.pomodoro_round == 0);
    assert(model.pending_break_min == 5);
    assert(pomodoro_model_skip_break(&model));
    assert(model.pomodoro_round == 1);
    assert(model.state == POMODORO_IDLE);

    /* Advance to round 3 via skip after focuses 2 and 3. */
    now_ms = 2000000;
    complete_focus(&model, now_ms);
    assert(model.pomodoro_round == 1);
    assert(model.pending_break_min == 5);
    assert(pomodoro_model_skip_break(&model));
    assert(model.pomodoro_round == 2);

    now_ms = 4000000;
    complete_focus(&model, now_ms);
    assert(model.pomodoro_round == 2);
    assert(pomodoro_model_skip_break(&model));
    assert(model.pomodoro_round == 3);

    /* Fourth focus: long break, round stays 3 until break completes. */
    now_ms = 6000000;
    assert(pomodoro_model_start_focus(&model, now_ms));
    uint64_t end_ms = now_ms + pomodoro_model_focus_min(&model) * 60000ULL;
    assert(pomodoro_model_tick(&model, end_ms) == POMODORO_EVENT_FOCUS_COMPLETE);
    assert(model.state == POMODORO_REWARD);
    assert(model.pomodoro_round == 3);
    assert(model.pending_break_min == 15);
    assert(pomodoro_model_tick(&model, end_ms + POMODORO_REWARD_MS) ==
           POMODORO_EVENT_REWARD_FINISHED);
    assert(model.state == POMODORO_BREAK_PROMPT);
    assert(model.pomodoro_round == 3);
    assert(pomodoro_model_start_break(&model, end_ms + POMODORO_REWARD_MS + 1));
    assert(model.state == POMODORO_BREAK_RUNNING);
    assert(model.pomodoro_round == 3);
    uint64_t break_end = end_ms + POMODORO_REWARD_MS + 1 + 15 * 60000ULL;
    assert(pomodoro_model_tick(&model, break_end) == POMODORO_EVENT_BREAK_COMPLETE);
    assert(model.state == POMODORO_IDLE);
    assert(model.pomodoro_round == 0);

    /* Abandon must not advance the round. */
    assert(pomodoro_model_start_focus(&model, break_end + 1000));
    assert(model.pomodoro_round == 0);
    assert(pomodoro_model_pause(&model, break_end + 2000));
    assert(pomodoro_model_request_abandon(&model, break_end + 3000));
    assert(pomodoro_model_confirm_abandon(&model));
    assert(model.state == POMODORO_IDLE);
    assert(model.pomodoro_round == 0);
}

int main(void) {
    test_defaults_and_selection();
    test_focus_pause_resume_and_completion();
    test_focus_pause_at_deadline_keeps_completion_event();
    test_abandon_timeout_and_confirm();
    test_break_cycle_and_growth();
    test_round_stays_through_reward_break_then_advances();
    test_break_timer();
    test_break_pause_at_deadline_keeps_completion_event();
    test_restore_is_safe();
    puts("pomodoro_model: all tests passed");
    return 0;
}
