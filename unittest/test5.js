load('jsonpath.js');

print('Checking large SQL statement...');

var json = '';
while (line = readline())
{
   json = json + line + '\n';
}

var json2 = eval('(' + json + ')');

if (jsonPath(json2, "$.data[0].rows[0].thisisaveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryverylongcolumn").toString() != '40')
{
   print('failed');
   quit(1);
}
print('passed');
quit(0);
