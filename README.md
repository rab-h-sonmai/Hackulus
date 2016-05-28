# Oculus software hack(s)

For now this is just a lousy proof-of-concept that shows how a 3rd-party application can change the Oculus software suite's settings to allow it to run without requiring the user to enable unknown applications.

![BLOB](http://imgur.com/KUS6lbT)

What this does:

 * It stops the ```Oculus VR Runtime Service``` service
 * Enumerates all folders within ```C:\Users\USERNAME\AppData\Roaming\Oculus\sessions\```
 * In every folder it opens the ```data.sqlite``` database
 * It extracts the BLOB stored in ```Objects.value``` where ```hashkey="__VIEWER_PRIMARY_KEY__"```
 * It finds the string ```are_unknown_applications_allowed``` and toggles the value thereof
 * It writes the BLOB back to the row
 * Finally it tries to start the Oculus service again.

*Note that you could just open the data.sqlite file, search for the string and change 3rd byte after it to 1, but then the database doesn't refresh.*

TODO:

* Make it less horrible (functions? what are those?)
* Port it to C# for Unity people
