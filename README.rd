= Requirements
* FUSE (>= 2.6)
* libfuse
* json-c
* glib-2.0
* libmagic
* libcurl

= Installation
1. Clone from github.
 $ git clone https://github.com/aru132/google-drive-on-fuse.git
2. Register this program with google developers console ( https://code.google.com/apis/console#access ) .
3. Download client_secret.json to %{HOME}/.config/fuse-google-drive/ .
4. Build the program.
 $ make clean;make
5. Run the program.
 $ ./google-drive-on-fuse /path/to/your/google/drive/directory/
6. Get the code and input the console.
7. If the program ended normally, the folder you selected may be mounted.
  * NOTE : The program may be down at SEGV at first login (and sometimes you run this program) . Then, try rerun the program.
    * This problem may be coming from google api's delay.  We will (and must) solve this problem in the future.
8. Enjoy!
