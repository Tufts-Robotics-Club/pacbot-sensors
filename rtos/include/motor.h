#pragma once

void motor_start(void);

// hands a message from the computer to motor_task. a newer one replaces one it hasnt gotten to yet
void motor_command(const char *text);
