#include <iostream>
#include <string>
#include <libnotify/notify.h>

class Notifier {
    private:
        NotifyNotification *notif;
        GError *error;
    public:
        Notifier() {
            error = NULL;
            notify_init("Audio Player");
            notif = notify_notification_new("Audio Player", "Audio Player", "Audio Player");
            notify_notification_show(notif, &error);
            if (error != NULL) std::cout << error->message || error->code;
        }
        ~Notifier() {
            notify_uninit();
        }
};
