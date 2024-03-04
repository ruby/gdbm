require "bundler/gem_tasks"
require "rake/testtask"

require 'rake/extensiontask'
extask = Rake::ExtensionTask.new("gdbm") do |x|
  x.lib_dir.sub!(%r[(?=/|\z)], "/#{RUBY_VERSION}/#{x.platform}")
end

Rake::TestTask.new(:test) do |t|
  t.libs = ["lib/#{RUBY_VERSION}/#{extask.platform}", "test/lib"]
  t.test_files = FileList['test/**/test_*.rb']
end

task :default => [:compile, :test]
