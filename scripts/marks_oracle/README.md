# marks_oracle — the #179 event-marks differential

Dumps the per-event `event_location` marks (0-based) from stdlib psych
and from the yeptris recorder for the same fixtures, then diffs them:

    ruby dump_psych.rb f2.yaml f3.yaml ...   # the ground truth
    cc -O1 dump_yeptris.c -I ../../src/include -I <build>/generated \
       <build>/src/libyeptris.a -o dump_yeptris
    ruby differ.rb f2.yaml f3.yaml ...

`test/unit/test_event_marks.cpp` freezes the psych output as the CI
gate. Scalar/alias END columns are pending the scalar-stamping slice
(tolerated in the gate; everything else asserts exactly).
