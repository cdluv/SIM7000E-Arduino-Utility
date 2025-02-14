Example of APN Manual configuration. 

Disable RF 
```
AT+CFUN=0 
+CPIN: NOT READY 
OK
```

Set theAPN manually.
Some operators need to set APN first when registering the network. 
```
AT+CGDCONT=1,"IP","globaldata.iot"
OK
```

Enable RF
```
AT+CFUN=1
OK
+CPIN: READY
```

Check PS service.
```
AT+CGATT?
+CGATT: 1
OK
```
	1 indicates PS has attached. 

Query the APN delivered by the network after the CAT-M or NB-IOT network is successfully registered. 
```
AT+CGNAPN
+CGNAPN: 1,"globaldata.iot" 
OK
```
	"ctnb" is APN delivered by the CAT-M or NB-IOT network.
	APN is empty under the GSM network. 

AT+CNCFG to set APN\username\password if needed. 

Before activation use
```
AT+CNCFG=0,1,"globaldata.iot"
OK
```

Activate network, Activate 0th PDP. 
```
AT+CNACT=0,1 
OK
+APP PDP: 0,ACTIVE

```

Get local IP
```
AT+CNACT?
+CNACT: 0,1,"10.185.48.73" 
+CNACT: 1,0,"0.0.0.0" 
+CNACT: 2,0,"0.0.0.0" 
+CNACT: 3,0,"0.0.0.0"
```

Build a TCP/UDP connection without SSL 

5.1.1 Build an ordinary TCP/UDP connection 

Example of Build an ordinary TCP/UDP connection. 

Check the 0th PDN/PDP local IP. 
```
AT+CNACT? 
+CNACT: 0,1,"10.185.48.73"" 
+CNACT: 1,0,"0.0.0.0" 
+CNACT: 2,0,"0.0.0.0" 
+CNACT: 3,0,"0.0.0.0" 
OK
```

About active PDN/PDP refer to Bearer Configuration. 

Set the 0th connection’s SSL enable option.  If TCP/UDP connection, the parameter is 0. This step can be omitted 

TURN OFF SSL
```
AT+CASSLCFG=0,"SSL",0
OK
```

Create a TCP connection with 0th PDP on 0th connection. 
Return to URC the first parameter is the identifier, the second parameter is the result of the connection, and the 0 indicates success. 
```

AT+CAOPEN=0,0,"TCP","139.59.180.17",8080
+CAOPEN: 0,0 
OK
```


```
AT+CASEND=0,5           // Request to send 5 bytes of data
>                       // Input Data here
OK                      // Data sent successfully
+CASEND: 0,0,5
CADATAIND: 0            //Data comes in on 0th connection.
```


Request to get 100 byte data sent by the server.
```
AT+CARECV=0,100
+CARECV: 10,GET / HTTP
OK
```

Close the connection with an identifier of 0
```
AT+CACLOSE=0
OK
```

Disconnect 0th data connection 
```
AT+CNACT=0,0
OK
+APP PDP: 0,DEACTIVE
```


