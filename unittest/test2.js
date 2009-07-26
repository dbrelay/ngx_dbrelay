load('jsonpath.js');

print('Checking mulitline query with comment...');

var json = '';
while (line = readline())
{
   json = json + line + '\n';
}

var json2 = eval('(' + json + ')');

if (jsonPath(json2, "$.data[0].rows[0]['1']").toString() != 42)
{
   print('failed');
   quit(1);
}
print('passed');
quit(0);

