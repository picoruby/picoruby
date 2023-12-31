#!/usr/bin/env ruby

# Usage:
#   If /dev/pts/18
#   ./vim.rb 18

require_relative './mrblib/vim.rb'
Vim.new("test.txt").start
