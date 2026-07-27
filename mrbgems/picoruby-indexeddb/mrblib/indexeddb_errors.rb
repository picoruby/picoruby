module IndexedDB
  class Error < StandardError; end

  # Raised when IndexedDB is not available in the current environment AND
  # the caller has explicitly disabled the in-memory fallback.
  class NotSupportedError < Error; end

  # Raised when an open() request fails (req.error or rejected promise).
  class OpenError < Error; end

  # Raised when versionchange stayed blocked past the bounded wait (another
  # connection held an older version of the database for too long).
  class BlockedError < Error; end

  # DOMException-backed errors from the tagged JS bridge. #name preserves the
  # DOMException name even for names that have no dedicated subclass here.
  class RequestError < Error
    attr_reader :name

    def initialize(name, message)
      @name = name
      super("#{name}: #{message}")
    end
  end

  # Availability errors: the browser context has no usable IndexedDB (private
  # mode and similar). These are the ONLY errors the in-memory fallback of
  # IndexedDB.open applies to.
  class SecurityError < RequestError; end
  class InvalidStateError < RequestError; end

  # Storage exists but the operation failed. NEVER silently replaced by the
  # in-memory fallback: substituting an empty store would masquerade as data
  # loss.
  class QuotaExceededError < RequestError; end
  class VersionError < RequestError; end
  class AbortError < RequestError; end

  DOM_ERROR_CLASSES = {
    "SecurityError" => SecurityError,
    "InvalidStateError" => InvalidStateError,
    "QuotaExceededError" => QuotaExceededError,
    "VersionError" => VersionError,
    "AbortError" => AbortError,
  }

  # Convert a tagged failure ({ ok: false, name:, message: }) into a typed
  # exception. Unknown DOMException names get the generic RequestError, with
  # the original name readable via #name.
  def self.error_for(name, message)
    klass = DOM_ERROR_CLASSES[name] || RequestError
    klass.new(name, message)
  end

  # Raised when Ruby code attempts to await a result inside a Database#batch
  # block. IDB transactions auto-commit at every JS event-loop turn, and every
  # JS::Promise#await yields to the event loop, so awaiting inside a batch
  # would deterministically kill the transaction.
  class AwaitInBatchError < Error; end

  # Raised when a schema mutation (create_store, delete_store, create_index)
  # is invoked outside an upgrade callback.
  class SchemaError < Error; end
end
