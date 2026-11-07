from datetime import datetime


# Returns the current human-readable time as YYYY-MM-DD HH:MM:SS.
def get_time():
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


# Prints a message designated as from the server.
def print_server_message(message):
    print(f"{get_time()} [SERVER]: {message}")
