<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Author: Jared Adolf-Bryfogle (jadolfbr@gmail.com)
A metric for measuring a TimingProfile and adding it to the resulting score file.  The time is between construction and calls of apply.  If you use it in RosettaScripts, you can have multiple timings between mover sets and determine the time between them using separate TimingProfileMetrics.   Useful to get runtimes of new movers and applications.

```xml
<TimingProfileMetric name="(&string;)" custom_type="(&string;)"
        hours="(false &bool;)" />
```

-   **custom_type**: Allows multiple configured SimpleMetrics of a single type to be called in a single RunSimpleMetrics and SimpleMetricFeatures. 
 The custom_type name will be added to the data tag in the scorefile or features database.
-   **hours**: Boolean to set whether we report in hours.  Default is minutes

---