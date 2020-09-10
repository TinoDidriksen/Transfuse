project = 'Transfuse'
copyright = ' 2020, Tino Didriksen'
author = 'Tino Didriksen <mail@tinodidriksen.com>'

extensions = [
]

#templates_path = ['templates']

exclude_patterns = ['build', 'Thumbs.db', '.DS_Store']

master_doc = 'index'

html_theme = 'sphinx_rtd_theme'

#html_static_path = ['static']

# Doesn't actually expand navigation by default, see https://github.com/readthedocs/sphinx_rtd_theme/issues/455
html_theme_options = {
	'collapse_navigation': False,
	'navigation_depth': 6,
}
