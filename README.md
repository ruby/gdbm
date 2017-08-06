# GDBM

[![Build Status](https://travis-ci.org/ruby/gdbm.svg?branch=master)](https://travis-ci.org/ruby/gdbm)

GNU dbm is a library for simple databases. A database is a file that stores
key-value pairs. Gdbm allows the user to store, retrieve, and delete data by
key. It furthermore allows a non-sorted traversal of all key-value pairs.
A gdbm database thus provides the same functionality as a hash. As
with objects of the Hash class, elements can be accessed with <tt>[]</tt>.
Furthermore, GDBM mixes in the Enumerable module, thus providing convenient
methods such as #find, #collect, #map, etc.

A process is allowed to open several different databases at the same time.
A process can open a database as a "reader" or a "writer". Whereas a reader
has only read-access to the database, a writer has read- and write-access.
A database can be accessed either by any number of readers or by exactly one
writer at the same time.

## Installation

Add this line to your application's Gemfile:

```ruby
gem 'gdbm'
```

And then execute:

    $ bundle

Or install it yourself as:

    $ gem install gdbm

## Usage


1. Opening/creating a database, and filling it with some entries:
require 'gdbm'

```ruby
gdbm = GDBM.new("fruitstore.db")
gdbm["ananas"]    = "3"
gdbm["banana"]    = "8"
gdbm["cranberry"] = "4909"
gdbm.close
```

2. Reading out a database:

```ruby
require 'gdbm'

gdbm = GDBM.new("fruitstore.db")
gdbm.each_pair do |key, value|
  print "#{key}: #{value}\n"
end
gdbm.close
```

## Contributing

Bug reports and pull requests are welcome on GitHub at https://github.com/ruby/gdbm.


## License

The gem is available as open source under the terms of the [2-Clause BSD License](https://opensource.org/licenses/BSD-2-Clause).
