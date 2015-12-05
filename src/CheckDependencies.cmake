# This module serve to figure out if there are all requiered dependencies
# on local machine and give you some tips, how to resolve known build issues

# To enable SSL or HTTP you are expected to have an OpenSSL library on
# your machine
if (SSL_ENABLED OR HTTP_ENABLED)
  message(STATUS "Looking for OpenSSL library...")
  find_package(OpenSSL QUIET)
  if(NOT OPENSSL_FOUND)
    message(STATUS "WARNING: can't find OpenSSL library! "
            "Now SSL_ENABLED and HTTP_ENABLED are OFF!")
    set(SSL_ENABLED   OFF)
    set(HTTP_ENABLED  OFF)
    message(STATUS "TIP: you may pass -DOPENSSL_ROOT_DIR=<openssl dir> as"
            " cmake argument to specify OpenSSL library directory manually")
  else(NOT OPENSSL_FOUND)
    message(STATUS "OpenSSL library was found!")
  endif(NOT OPENSSL_FOUND)
endif(SSL_ENABLED OR HTTP_ENABLED)
