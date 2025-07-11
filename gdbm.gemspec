# coding: utf-8
# frozen_string_literal: true

Gem::Specification.new do |spec|
  spec.name          = "gdbm"
  spec.version       = "2.1.1"
  spec.authors       = ["Yukihiro Matsumoto"]
  spec.email         = ["matz@ruby-lang.org"]

  spec.summary       = "Ruby extension for GNU dbm."
  spec.description   = "Ruby extension for GNU dbm."
  spec.homepage      = "https://github.com/ruby/gdbm"
  spec.licenses      = ["GPL-2.0-or-later"]

  spec.files         = ["ext/gdbm/extconf.rb", "ext/gdbm/gdbm.c"]
  spec.bindir        = "exe"
  spec.executables   = []
  spec.require_paths = ["lib"]
  spec.extensions    = ["ext/gdbm/extconf.rb"]
  spec.required_ruby_version = ">= 2.3.0"
  spec.metadata["msys2_mingw_dependencies"] = "gdbm"
end
