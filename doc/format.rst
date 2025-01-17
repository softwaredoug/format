.. highlight:: c++

.. _string-formatting:

String Formatting
-----------------

.. doxygenfunction:: format::Format(StringRef)

.. doxygenclass:: format::Formatter
   :members:

.. doxygenclass:: format::StringRef
   :members:

.. ifconfig:: False

   .. class:: Formatter

      In addition, the :class:`Formatter` defines a number of methods that are
      intended to be replaced by subclasses:

      .. method:: parse(format_string)

         Loop over the format_string and return an iterable of tuples
         (*literal_text*, *field_name*, *format_spec*, *conversion*).  This is used
         by :meth:`vformat` to break the string into either literal text, or
         replacement fields.

         The values in the tuple conceptually represent a span of literal text
         followed by a single replacement field.  If there is no literal text
         (which can happen if two replacement fields occur consecutively), then
         *literal_text* will be a zero-length string.  If there is no replacement
         field, then the values of *field_name*, *format_spec* and *conversion*
         will be ``None``.

      .. method:: get_field(field_name, args, kwargs)

         Given *field_name* as returned by :meth:`parse` (see above), convert it to
         an object to be formatted.  Returns a tuple (obj, used_key).  The default
         version takes strings of the form defined in :pep:`3101`, such as
         "0[name]" or "label.title".  *args* and *kwargs* are as passed in to
         :meth:`vformat`.  The return value *used_key* has the same meaning as the
         *key* parameter to :meth:`get_value`.

      .. method:: get_value(key, args, kwargs)

         Retrieve a given field value.  The *key* argument will be either an
         integer or a string.  If it is an integer, it represents the index of the
         positional argument in *args*; if it is a string, then it represents a
         named argument in *kwargs*.

         The *args* parameter is set to the list of positional arguments to
         :meth:`vformat`, and the *kwargs* parameter is set to the dictionary of
         keyword arguments.

         For compound field names, these functions are only called for the first
         component of the field name; Subsequent components are handled through
         normal attribute and indexing operations.

         So for example, the field expression '0.name' would cause
         :meth:`get_value` to be called with a *key* argument of 0.  The ``name``
         attribute will be looked up after :meth:`get_value` returns by calling the
         built-in :func:`getattr` function.

         If the index or keyword refers to an item that does not exist, then an
         :exc:`IndexError` or :exc:`KeyError` should be raised.

      .. method:: check_unused_args(used_args, args, kwargs)

         Implement checking for unused arguments if desired.  The arguments to this
         function is the set of all argument keys that were actually referred to in
         the format string (integers for positional arguments, and strings for
         named arguments), and a reference to the *args* and *kwargs* that was
         passed to vformat.  The set of unused args can be calculated from these
         parameters.  :meth:`check_unused_args` is assumed to raise an exception if
         the check fails.

      .. method:: format_field(value, format_spec)

         :meth:`format_field` simply calls the global :func:`format` built-in.  The
         method is provided so that subclasses can override it.

      .. method:: convert_field(value, conversion)

         Converts the value (returned by :meth:`get_field`) given a conversion type
         (as in the tuple returned by the :meth:`parse` method).  The default
         version understands 's' (str), 'r' (repr) and 'a' (ascii) conversion
         types.


.. _formatstrings:

Format String Syntax
--------------------

The :cpp:func:`format::Format()` function and the :cpp:class:`format::Formatter`
class share the same syntax for format strings.

Format strings contain "replacement fields" surrounded by curly braces ``{}``.
Anything that is not contained in braces is considered literal text, which is
copied unchanged to the output.  If you need to include a brace character in the
literal text, it can be escaped by doubling: ``{{`` and ``}}``.

The grammar for a replacement field is as follows:

   .. productionlist:: sf
      replacement_field: "{" [`arg_index`] [":" `format_spec`] "}"
      arg_index: `integer`

In less formal terms, the replacement field can start with an *arg_index*
that specifies the argument whose value is to be formatted and inserted into
the output instead of the replacement field.
The *arg_index* is optionally followed by a *format_spec*, which is preceded
by a colon ``':'``.  These specify a non-default format for the replacement value.

See also the :ref:`formatspec` section.

If the numerical arg_indexes in a format string are 0, 1, 2, ... in sequence,
they can all be omitted (not just some) and the numbers 0, 1, 2, ... will be
automatically inserted in that order.

Some simple format string examples::

   "First, thou shalt count to {0}" // References the first argument
   "Bring me a {}"                  // Implicitly references the first argument
   "From {} to {}"                  // Same as "From {0} to {1}"

The *format_spec* field contains a specification of how the value should be
presented, including such details as field width, alignment, padding, decimal
precision and so on.  Each value type can define its own "formatting
mini-language" or interpretation of the *format_spec*.

Most built-in types support a common formatting mini-language, which is
described in the next section.

A *format_spec* field can also include nested replacement fields within it.
These nested replacement fields can contain only an argument index;
format specifications are not allowed.  Formatting is performed as if the
replacement fields within the format_spec are substituted before the
*format_spec* string is interpreted.  This allows the formatting of a value
to be dynamically specified.

See the :ref:`formatexamples` section for some examples.


.. _formatspec:

Format Specification Mini-Language
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

"Format specifications" are used within replacement fields contained within a
format string to define how individual values are presented (see
:ref:`formatstrings`).  They can also be passed directly to the
:func:`Format` function.  Each formattable type may define how the format
specification is to be interpreted.

Most built-in types implement the following options for format specifications,
although some of the formatting options are only supported by the numeric types.

The general form of a *standard format specifier* is:

.. productionlist:: sf
   format_spec: [[`fill`]`align`][`sign`]["#"]["0"][`width`]["." `precision`][`type`]
   fill: <a character other than '{' or '}'>
   align: "<" | ">" | "=" | "^"
   sign: "+" | "-" | " "
   width: `integer`
   precision: `integer` | "{" `arg_index` "}"
   type: "c" | "d" | "e" | "E" | "f" | "F" | "g" | "G" | "o" | "p" | s" | "x" | "X"

The *fill* character can be any character other than '{' or '}'.  The presence
of a fill character is signaled by the character following it, which must be
one of the alignment options.  If the second character of *format_spec* is not
a valid alignment option, then it is assumed that both the fill character and
the alignment option are absent.

The meaning of the various alignment options is as follows:

   +---------+----------------------------------------------------------+
   | Option  | Meaning                                                  |
   +=========+==========================================================+
   | ``'<'`` | Forces the field to be left-aligned within the available |
   |         | space (this is the default for most objects).            |
   +---------+----------------------------------------------------------+
   | ``'>'`` | Forces the field to be right-aligned within the          |
   |         | available space (this is the default for numbers).       |
   +---------+----------------------------------------------------------+
   | ``'='`` | Forces the padding to be placed after the sign (if any)  |
   |         | but before the digits.  This is used for printing fields |
   |         | in the form '+000000120'. This alignment option is only  |
   |         | valid for numeric types.                                 |
   +---------+----------------------------------------------------------+
   | ``'^'`` | Forces the field to be centered within the available     |
   |         | space.                                                   |
   +---------+----------------------------------------------------------+

Note that unless a minimum field width is defined, the field width will always
be the same size as the data to fill it, so that the alignment option has no
meaning in this case.

The *sign* option is only valid for number types, and can be one of the
following:

   +---------+----------------------------------------------------------+
   | Option  | Meaning                                                  |
   +=========+==========================================================+
   | ``'+'`` | indicates that a sign should be used for both            |
   |         | positive as well as negative numbers.                    |
   +---------+----------------------------------------------------------+
   | ``'-'`` | indicates that a sign should be used only for negative   |
   |         | numbers (this is the default behavior).                  |
   +---------+----------------------------------------------------------+
   | space   | indicates that a leading space should be used on         |
   |         | positive numbers, and a minus sign on negative numbers.  |
   +---------+----------------------------------------------------------+


The ``'#'`` option causes the "alternate form" to be used for the
conversion.  The alternate form is defined differently for different
types.  This option is only valid for integer and floating-point types.
For integers, when octal, or hexadecimal output
is used, this option adds the prefix respective ``'0'``, or
``'0x'`` to the output value. For floating-point numbers the
alternate form causes the result of the conversion to always contain a
decimal-point character, even if no digits follow it. Normally, a
decimal-point character appears in the result of these conversions
only if a digit follows it. In addition, for ``'g'`` and ``'G'``
conversions, trailing zeros are not removed from the result.

.. ifconfig:: False

   The ``','`` option signals the use of a comma for a thousands separator.
   For a locale aware separator, use the ``'n'`` integer presentation type
   instead.

*width* is a decimal integer defining the minimum field width.  If not
specified, then the field width will be determined by the content.

Preceding the *width* field by a zero (``'0'``) character enables
sign-aware zero-padding for numeric types.  This is equivalent to a *fill*
character of ``'0'`` with an *alignment* type of ``'='``.

The *precision* is a decimal number indicating how many digits should be
displayed after the decimal point for a floating point value formatted with
``'f'`` and ``'F'``, or before and after the decimal point for a floating point
value formatted with ``'g'`` or ``'G'``.  For non-number types the field
indicates the maximum field size - in other words, how many characters will be
used from the field content. The *precision* is not allowed for integer values.

Finally, the *type* determines how the data should be presented.

The available string presentation types are:

   +---------+----------------------------------------------------------+
   | Type    | Meaning                                                  |
   +=========+==========================================================+
   | ``'s'`` | String format. This is the default type for strings and  |
   |         | may be omitted.                                          |
   +---------+----------------------------------------------------------+
   | none    | The same as ``'s'``.                                     |
   +---------+----------------------------------------------------------+

The available character presentation types are:

   +---------+----------------------------------------------------------+
   | Type    | Meaning                                                  |
   +=========+==========================================================+
   | ``'c'`` | Character format. This is the default type for           |
   |         | characters and may be omitted.                           |
   +---------+----------------------------------------------------------+
   | none    | The same as ``'c'``.                                     |
   +---------+----------------------------------------------------------+

The available integer presentation types are:

   +---------+----------------------------------------------------------+
   | Type    | Meaning                                                  |
   +=========+==========================================================+
   | ``'d'`` | Decimal Integer. Outputs the number in base 10.          |
   +---------+----------------------------------------------------------+
   | ``'o'`` | Octal format. Outputs the number in base 8.              |
   +---------+----------------------------------------------------------+
   | ``'x'`` | Hex format. Outputs the number in base 16, using         |
   |         | lower-case letters for the digits above 9.               |
   +---------+----------------------------------------------------------+
   | ``'X'`` | Hex format. Outputs the number in base 16, using         |
   |         | upper-case letters for the digits above 9.               |
   +---------+----------------------------------------------------------+
   | none    | The same as ``'d'``.                                     |
   +---------+----------------------------------------------------------+

The available presentation types for floating point values are:

   +---------+----------------------------------------------------------+
   | Type    | Meaning                                                  |
   +=========+==========================================================+
   | ``'e'`` | Exponent notation. Prints the number in scientific       |
   |         | notation using the letter 'e' to indicate the exponent.  |
   +---------+----------------------------------------------------------+
   | ``'E'`` | Exponent notation. Same as ``'e'`` except it uses an     |
   |         | upper case 'E' as the separator character.               |
   +---------+----------------------------------------------------------+
   | ``'f'`` | Fixed point. Displays the number as a fixed-point        |
   |         | number.                                                  |
   +---------+----------------------------------------------------------+
   | ``'F'`` | Fixed point. Same as ``'f'``, but converts ``nan`` to    |
   |         | ``NAN`` and ``inf`` to ``INF``.                          |
   +---------+----------------------------------------------------------+
   | ``'g'`` | General format.  For a given precision ``p >= 1``,       |
   |         | this rounds the number to ``p`` significant digits and   |
   |         | then formats the result in either fixed-point format     |
   |         | or in scientific notation, depending on its magnitude.   |
   |         |                                                          |
   |         | A precision of ``0`` is treated as equivalent to a       |
   |         | precision of ``1``.                                      |
   +---------+----------------------------------------------------------+
   | ``'G'`` | General format. Same as ``'g'`` except switches to       |
   |         | ``'E'`` if the number gets too large. The                |
   |         | representations of infinity and NaN are uppercased, too. |
   +---------+----------------------------------------------------------+
   | none    | The same as ``'g'``.                                     |
   +---------+----------------------------------------------------------+

.. ifconfig:: False

   +---------+----------------------------------------------------------+
   |         | The precise rules are as follows: suppose that the       |
   |         | result formatted with presentation type ``'e'`` and      |
   |         | precision ``p-1`` would have exponent ``exp``.  Then     |
   |         | if ``-4 <= exp < p``, the number is formatted            |
   |         | with presentation type ``'f'`` and precision             |
   |         | ``p-1-exp``.  Otherwise, the number is formatted         |
   |         | with presentation type ``'e'`` and precision ``p-1``.    |
   |         | In both cases insignificant trailing zeros are removed   |
   |         | from the significand, and the decimal point is also      |
   |         | removed if there are no remaining digits following it.   |
   |         |                                                          |
   |         | Positive and negative infinity, positive and negative    |
   |         | zero, and nans, are formatted as ``inf``, ``-inf``,      |
   |         | ``0``, ``-0`` and ``nan`` respectively, regardless of    |
   |         | the precision.                                           |
   |         |                                                          |
   +---------+----------------------------------------------------------+

The available presentation types for pointers are:

   +---------+----------------------------------------------------------+
   | Type    | Meaning                                                  |
   +=========+==========================================================+
   | ``'p'`` | Pointer format. This is the default type for             |
   |         | pointers and may be omitted.                             |
   +---------+----------------------------------------------------------+
   | none    | The same as ``'p'``.                                     |
   +---------+----------------------------------------------------------+


.. _formatexamples:

Format examples
^^^^^^^^^^^^^^^

This section contains examples of the format syntax and comparison with
the printf formatting.

In most of the cases the syntax is similar to the printf formatting, with the
addition of the ``{}`` and with ``:`` used instead of ``%``.
For example, ``"%03.2f"`` can be translated to ``"{:03.2f}"``.

The new format syntax also supports new and different options, shown in the
following examples.

Accessing arguments by position::

   Format("{0}, {1}, {2}") << 'a' << 'b' << 'c';
   // Result: "a, b, c"
   Format("{}, {}, {}") << 'a' << 'b' << 'c';
   // Result: "a, b, c"
   Format("{2}, {1}, {0}") << 'a' << 'b' << 'c';
   // Result: "c, b, a"
   Format("{0}{1}{0}") << "abra" << "cad";  // arguments' indices can be repeated
   // Result: "abracadabra"

Aligning the text and specifying a width::

   Format("{:<30}") << "left aligned";
   // Result: "left aligned                  "
   Format("{:>30}") << "right aligned"
   // Result: "                 right aligned"
   Format("{:^30}") << "centered"
   // Result: "           centered           "
   Format("{:*^30}") << "centered"  // use '*' as a fill char
   // Result: "***********centered***********"

Replacing ``%+f``, ``%-f``, and ``% f`` and specifying a sign::

   Format("{:+f}; {:+f}") << 3.14 << -3.14;  // show it always
   // Result: "+3.140000; -3.140000"
   Format("{: f}; {: f}") << 3.14 << -3.14;  // show a space for positive numbers
   // Result: " 3.140000; -3.140000"
   Format("{:-f}; {:-f}") << 3.14 << -3.14;  // show only the minus -- same as '{:f}; {:f}'
   // Result: "3.140000; -3.140000"

Replacing ``%x`` and ``%o`` and converting the value to different bases::

   Format("int: {0:d};  hex: {0:x};  oct: {0:o}") << 42;
   // Result: "int: 42;  hex: 2a;  oct: 52"
   // with 0x or 0 as prefix:
   Format("int: {0:d};  hex: {0:#x};  oct: {0:#o}") << 42;
   // Result: "int: 42;  hex: 0x2a;  oct: 052"

.. ifconfig:: False

   Using the comma as a thousands separator::

      Format("{:,}") << 1234567890)
      '1,234,567,890'

   Expressing a percentage::

      >>> points = 19
      >>> total = 22
      Format("Correct answers: {:.2%}") << points/total)
      'Correct answers: 86.36%'

   Using type-specific formatting::

      >>> import datetime
      >>> d = datetime.datetime(2010, 7, 4, 12, 15, 58)
      Format("{:%Y-%m-%d %H:%M:%S}") << d)
      '2010-07-04 12:15:58'

   Nesting arguments and more complex examples::

      >>> for align, text in zip('<^>', ['left', 'center', 'right']):
      ...     '{0:{fill}{align}16}") << text, fill=align, align=align)
      ...
      'left<<<<<<<<<<<<'
      '^^^^^center^^^^^'
      '>>>>>>>>>>>right'
      >>>
      >>> octets = [192, 168, 0, 1]
      Format("{:02X}{:02X}{:02X}{:02X}") << *octets)
      'C0A80001'
      >>> int(_, 16)
      3232235521
      >>>
      >>> width = 5
      >>> for num in range(5,12):
      ...     for base in 'dXob':
      ...         print('{0:{width}{base}}") << num, base=base, width=width), end=' ')
      ...     print()
      ...
          5     5     5   101
          6     6     6   110
          7     7     7   111
          8     8    10  1000
          9     9    11  1001
         10     A    12  1010
         11     B    13  1011

