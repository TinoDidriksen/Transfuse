#########
Transfuse
#########

.. toctree::
	:caption: Contents:


* :ref:`genindex`
* :ref:`search`

******************
What is Transfuse?
******************

`Transfuse <https://github.com/TinoDidriksen/Transfuse>`_ takes a document with nested inline complex formatting and extracts this formatting into manageable spans, then optionally sends the resulting stream to a handler for transformation, then injects the transformed stream back into the original document and turns the spans back into formatting.

Only usable text is sent through the handler, so that it does not need to care about maintaining XML or other non-textual structures. And due to the way Transfuse converts to and from the spans, it does not matter if the handler discards, moves, duplicates, or splits the spans.

The handler step is optional. It can be useful to run Transfuse on its own on a document to prepare for (semi-)manual translation via CAT tools such as `OmegaT <https://omegat.org/>`_, as that cleans up formatting and removes superfluous information that would otherwise foil translation memory likeness and be in the way for humans.

Supported Document Formats
==========================

* :ref:`fmt-html`
* :ref:`fmt-html-fragment`
* :ref:`fmt-docx`
* :ref:`fmt-pptx`
* :ref:`fmt-odt` (LibreOffice / OpenOffice.org)
* :ref:`fmt-txt`

Indirectly Supported Formats
----------------------------

For textual documents that are not XML-based, such as old Microsoft Word DOC and WordPerfect WPF, you can use `LibreOffice <https://ask.libreoffice.org/en/question/1686/how-to-not-connect-to-a-running-instance/>`_ or `AbiWord <https://www.abisource.com/wiki/AbiCommand>`_ in headless mode to convert them to Open Document Format (ODT) and then process them with Transfuse.

Unsupported Formats
-------------------

* Spreadsheets (XLSX, ODS):
  Not prioritised as they rarely contain translatable information. But would be easy to add if anyone really wants it.
* SDL Trados TTX:
  Got code laying around for this format, but not implemented in Transfuse yet.
* MediaWiki:
  Only MediaWiki can correctly parse the MediaWiki format. Use their `Content Translation <https://www.mediawiki.org/wiki/Content_translation>`_ extension to get HTML in and out instead.
* PDF:
  Use pdftotext or pdftohtml first (found in `Xpdf <https://www.xpdfreader.com/>`_ or `Poppler <https://poppler.freedesktop.org/>`_), then pass the result to Transfuse.
* …etc:
  If the format is XML-based or trivially convertible to XML, then it should be easy to add support for it to Transfuse. For other formats, use helper tools to convert to a usable format.

Implementation Details
======================

In broad strokes, the data flow is:

1. Extraction

  a) The input file or stream is copied to a temporary state folder.
  #) If not told which format the input file is in, try to auto-detect it.
  #) Format-specific preprocessing. See details for each format below.
  #) Walk the XML tree and record all whitespace as attributes on the closest tags.
  #) Serialize the XML tree while turning all inline tags into simpler spans.
  #) Extract all text blocks from the simplified XML document.

2. Transformation Handler
3. Injection

The whole input file is copied because the full document must be available in pristine state throughout the whole process. Also because the process can be interrupted, so the handler or injection steps may happen at a later time or on different machines.

.. _fmt-html:

HTML
----

.. _fmt-html-fragment:

HTML Fragment
-------------

.. _fmt-docx:

Microsoft Word (DOCX)
---------------------

.. _fmt-pptx:

Microsoft PowerPoint (PPTX)
---------------------------

.. _fmt-odt:

Open Document Format (ODT, ODP)
-------------------------------

.. _fmt-txt:

Plain Text (TXT)
----------------

