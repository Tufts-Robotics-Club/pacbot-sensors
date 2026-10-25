#pragma once

// wifi, websocket server and the task that sends replies
void comms_start(void);

// sends text back to whoever is sending us commands. fine to call from motor_task
void comms_send(const char *text);
