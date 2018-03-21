# uWSGI

uWSGI fork with fixed path handling for applications in subdirectories.

## Install

Install the Apache headers

    # apt install apache2-dev

Clone the repo

    $ git clone https://github.com/Yepoleb/uwsgi.git
    $ cd uwsgi/apache2

Build the module and install it

    # apxs2 -i -c mod_proxy_uwsgi.c

Restart Apache2

    # systemctl restart apache2

