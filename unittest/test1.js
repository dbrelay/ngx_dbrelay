load('jsonpath.js');

print('Checking empty result set...');

var json = '';
while (line = readline())
{
   json = json + line + '\n';
}

//print(json);
var json2 = eval('(' + json + ')');

//print(jsonPath(json2, '$..sql_database').toString());
if (jsonPath(json2, '$.data[0].rows[0].txt').toString() != 'false')
{
   print('failed');
   quit(1);
}
if (jsonPath(json2, '$.data[1].rows[0].txt').toString() != 'GOT IT!') 
{
   print('failed');
   quit(2);
}
print('passed');
quit(0);

