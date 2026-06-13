module IndexedDB
  class Error < StandardError; end

  # Raised when IndexedDB is not available in the current environment AND
  # the caller has explicitly disabled the in-memory fallback.
  class NotSupportedError < Error; end

  # Raised when an open() request fails (req.error or rejected promise).
  class OpenError < Error; end

  # Raised when versionchange is blocked because another connection still
  # holds an older version of the database.
  class BlockedError < Error; end

  # Raised when Ruby code attempts to await a result inside a Database#batch
  # block. IDB transactions auto-commit at every JS event-loop turn, and every
  # JS::Promise#await yields to the event loop, so awaiting inside a batch
  # would deterministically kill the transaction.
  class AwaitInBatchError < Error; end

  # Raised when a schema mutation (create_store, delete_store, create_index)
  # is invoked outside an upgrade callback.
  class SchemaError < Error; end
end
