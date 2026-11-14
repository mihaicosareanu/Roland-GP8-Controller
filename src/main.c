#include "hal.h"
#include "app.h"

int main(void)
{
    hal_init();
    app_init();
    for (;;) {
        app_task();
    }
}
