poki~ is a Max/MSP external that combines the features of the built-in object poke~ and index~. in addition it provides:
- interpolation on both input and output, allowing for oversampling.
- overdubbing.
- input scaling.
- parameter smoothing.

a (slightly) fuller description is provided in the help patch.

building the project requires the Max/MSP software development kit, available here:
http://cycling74.com/products/sdk/

the poki~ project folder should live the same folder as MaxSDK-5.x.x/

the project is set to use version 5.1.6 of the SDK. if you want to use a different version, or keep it in a different location, edit the C74SUPPORT variable in poki~/maxmspsdx.xconfig. edit DSTROOT if you want a different location for the build destination.

designed by jamie lidell, brian crabtree, and ezra buchla
implemented by ezra buchla, 2011
contact@catfact.net
thanks!