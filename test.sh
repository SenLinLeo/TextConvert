
#!/bin/bash
./tool -i test/input/1.txt -o test/output/1.txt && echo "TEST PASS" || echo "TEST FAILED"
./tool -i test/input/2.txt -o test/output/2.txt && echo "TEST PASS" || echo "TEXT FAILED"
