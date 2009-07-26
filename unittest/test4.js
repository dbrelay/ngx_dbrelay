load('jsonpath.js');

print('Checking error messages...');

var json = '';
while (line = readline())
{
   json = json + line + '\n';
}

var json2 = eval('(' + json + ')');

if (jsonPath(json2, "$.data[0].rows").toString() != 'false')
{
   print('failed');
   quit(1);
}
if (jsonPath(json2, "$.log.error").toString() == 'false')
{
   print(jsonPath(json2, "$.log.error").toString());
   print('failed');
   quit(2);
}
print('passed');
quit(0);
