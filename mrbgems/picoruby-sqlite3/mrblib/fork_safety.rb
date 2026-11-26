class SQLite3
  # The sqlite3 gem's ForkSafety warns when a database handle is carried
  # across fork(). There is no fork() here, so there is nothing to track
  # and nothing to silence; the methods exist for code written to the gem.
  module ForkSafety
    def self.suppress_warnings!
      nil
    end
  end
end
