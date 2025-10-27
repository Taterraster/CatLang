# Tutorial
```cat
purr ~> "Hello, World";
```
outputs Hello, World
```
str <name> ~> "<value>";
```
defines a string
```
num <name> ~> "<value>";
```
defines an intiger
```
bool <name> ~> <value>;
```
defines a boolean
```
<returnType> <funcName> (<arguments>) {
    <code>
}
```
makes a function
```
<funcName>(<args>);
```
runs a function
```
// This is a comment
/*
this
is
a
multi
line
comment
*/
```
```
str name ~> "Motchi";
num age ~> 2;
num weight ~> 4.5;

purr ~> "name: " + name;
purr ~> "Age: " + age;
purr ~> "Weight: " + weight + "kg";
```
simple script