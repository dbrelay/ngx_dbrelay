load('jsonpath.js');

print('Checking text columns...');

var json = '';
while (line = readline())
{
   json = json + line + '\n';
}

print(json);
var json2 = eval('(' + json + ')');

if (jsonPath(json2, "$.data[1].rows[0].t").toString() != 'Text Text Text')
{
   print('failed');
   quit(1);
}
print('passed');
quit(0);
