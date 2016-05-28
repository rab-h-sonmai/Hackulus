# Oculus software hack(s)

For now this is just a lousy proof-of-concept that shows how a 3rd-party application can change the Oculus software suite's settings to allow it to run without requiring the user to enable unknown applications.

![BLOB](http://i.imgur.com/5t5aEBi.png)

What this does:

 * It stops the ```Oculus VR Runtime Service``` service
 * Enumerates all folders within ```C:\Users\USERNAME\AppData\Roaming\Oculus\sessions\```
 * In every folder it opens the ```data.sqlite``` database
 * It extracts the BLOB stored in ```Objects.value``` where ```hashkey="__VIEWER_PRIMARY_KEY__"```
 * It finds the string ```are_unknown_applications_allowed``` and toggles the value thereof
 * It writes the BLOB back to the row
 * Finally it tries to start the Oculus service again.

*Note that you could just open the data.sqlite file, search for the string and change 3rd byte after it to 1, but then the database doesn't refresh.*

Requirements: SQLite  
License: MIT  
Disclaimer: I am not responsible for any damage or functionality loss that results from using this software.  
The program does attempt to backup the BLOB it replaces, so should anything go wrong... well, you have the original BLOB you can write back to the database.

TODO:

* Make it less horrible (functions? what are those?)
* Port it to C# for Unity people
* Add "restore original BLOB" functionality.
* 
