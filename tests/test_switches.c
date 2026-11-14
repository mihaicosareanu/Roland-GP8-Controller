#include "check.h"
#include "switches.h"

#define B(n) (1u << ((n) - 1))

int main(void)
{
    switches_t s; sw_event_t ev;

    /* debounce: 29 ms is not enough, 30 ms is */
    switches_init(&s, 0, 0);
    CHECK(!switches_update(&s, 100, B(3), false, &ev));
    CHECK(!switches_update(&s, 129, B(3), false, &ev));
    CHECK(switches_update(&s, 130, B(3), false, &ev) && ev.sw == 3 && !ev.sw10_held);

    /* bounce restarts the timer */
    switches_init(&s, 0, 0);
    switches_update(&s, 100, B(1), false, &ev);
    switches_update(&s, 120, 0, false, &ev);
    CHECK(!switches_update(&s, 140, B(1), false, &ev));
    CHECK(!switches_update(&s, 169, B(1), false, &ev));
    CHECK(switches_update(&s, 170, B(1), false, &ev) && ev.sw == 1);

    /* first press wins, others ignored until ALL released */
    switches_init(&s, 0, 0);
    switches_update(&s, 100, B(5), false, &ev);
    CHECK(switches_update(&s, 130, B(5), false, &ev) && ev.sw == 5);
    switches_update(&s, 140, B(5) | B(2), false, &ev);
    CHECK(!switches_update(&s, 180, B(5) | B(2), false, &ev));        /* SW_2 ignored */
    CHECK(!switches_update(&s, 200, B(2), false, &ev));               /* SW_5 released, SW_2 still held */
    CHECK(!switches_update(&s, 240, B(2), false, &ev));
    CHECK(!switches_update(&s, 250, 0, false, &ev));
    CHECK(!switches_update(&s, 290, 0, false, &ev));                  /* all released: lockout over */
    switches_update(&s, 300, B(7), false, &ev);
    CHECK(switches_update(&s, 330, B(7), false, &ev) && ev.sw == 7);

    /* two presses becoming valid together: lowest number wins */
    switches_init(&s, 0, 0);
    switches_update(&s, 100, B(8) | B(4), false, &ev);
    CHECK(switches_update(&s, 130, B(8) | B(4), false, &ev) && ev.sw == 4);

    /* held at power-up: ignored until released */
    switches_init(&s, 0, B(3));
    CHECK(!switches_update(&s, 50, B(3), false, &ev));
    switches_update(&s, 60, 0, false, &ev);
    CHECK(!switches_update(&s, 100, 0, false, &ev));
    switches_update(&s, 110, B(4), false, &ev);
    CHECK(switches_update(&s, 140, B(4), false, &ev) && ev.sw == 4);

    /* combination: SW_10 first, then one of SW_1..8 while SW_10 held */
    switches_init(&s, 0, 0);
    switches_update(&s, 100, B(10), true, &ev);
    CHECK(switches_update(&s, 130, B(10), true, &ev) && ev.sw == 10);
    switches_update(&s, 140, B(10) | B(3), true, &ev);
    CHECK(switches_update(&s, 170, B(10) | B(3), true, &ev) && ev.sw == 3 && ev.sw10_held);
    switches_update(&s, 180, B(10) | B(3) | B(6), true, &ev);
    CHECK(!switches_update(&s, 220, B(10) | B(3) | B(6), true, &ev)); /* only one second switch */

    /* combination disabled outside PROGRAM_MODE */
    switches_init(&s, 0, 0);
    switches_update(&s, 100, B(10), false, &ev);
    switches_update(&s, 130, B(10), false, &ev);
    switches_update(&s, 140, B(10) | B(3), false, &ev);
    CHECK(!switches_update(&s, 180, B(10) | B(3), false, &ev));

    /* reverse order is not a combination */
    switches_init(&s, 0, 0);
    switches_update(&s, 100, B(3), true, &ev);
    switches_update(&s, 130, B(3), true, &ev);
    switches_update(&s, 140, B(3) | B(10), true, &ev);
    CHECK(!switches_update(&s, 180, B(3) | B(10), true, &ev));

    REPORT("test_switches");
}
