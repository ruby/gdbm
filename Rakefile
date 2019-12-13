require "bundler/gem_tasks"
require "rake/testtask"

Rake::TestTask.new(:test) do |t|
  t.libs << "test/lib"
  t.pattern = 'test/**/test_*.rb'
  t.options = '--no-show-detail-immediately'
  t.ruby_opts = %w[-v]
end

require 'rake/extensiontask'
Rake::ExtensionTask.new("gdbm")

task :default => [:compile, :test]
