load('jsonpath.js');

print('Checking column overrun...');

var json = '';
while (line = readline())
{
   json = json + line + '\n';
}

var json2 = eval('(' + json + ')');

if (jsonPath(json2, "$.data[1].rows[0].first").toString() != '12345678901234567890123456789012')
{
   print('failed');
   quit(1);
}
if (jsonPath(json2, "$.data[1].rows[0].second").toString() != '-')
{
   print('failed');
   quit(2);
}
print('passed');
quit(0);
