load('jsonpath.js');

print('Checking calculated columns...');

var json = '';
while (line = readline())
{
   json = json + line + '\n';
}

var json2 = eval('(' + json + ')');

if (jsonPath(json2, "$.data[0].rows[0]['1']").toString() != 3)
{
   print('failed');
   quit(1);
}
if (jsonPath(json2, "$.data[0].rows[0]['2']").toString() != 2)
{
   print('failed');
   quit(1);
}
if (jsonPath(json2, "$.data[0].rows[0]['3']").toString() != 1)
{
   print('failed');
   quit(1);
}
print('passed');
quit(0);

