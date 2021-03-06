// Copyright (c) 2015, the Fletch project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.md file.

library fletchc.verbs.help_verb;

import 'infrastructure.dart';

import 'verbs.dart' show
    commonVerbs,
    uncommonVerbs;

import 'documentation.dart' show
    helpDocumentation;

const Verb helpVerb =
    const Verb(
        help, helpDocumentation,
        supportedTargets: const [ TargetKind.ALL ], allowsTrailing: true);

Future<int> help(AnalyzedSentence sentence, _) async {
  int exitCode = 0;
  bool showAllVerbs = sentence.target != null;
  if (sentence.trailing != null) {
    exitCode = 1;
  }
  if (sentence.verb.name != "help") {
    exitCode = 1;
  }
  print(generateHelpText(showAllVerbs));
  return exitCode;
}

String generateHelpText(bool showAllVerbs) {
  List<String> helpStrings = <String>[];
  bool isFirst = true;
  addVerb(String name, Verb verb) {
    if (!isFirst) helpStrings.add("");
    isFirst = false;
    List<String> lines = verb.documentation.trimRight().split("\n");
    for (int i = 0; i < lines.length; i++) {
      String line = lines[i];
      if (line.length > 80) {
        throw new StateError(
            "Line ${i+1} of Verb '$name' is too long and may not be "
            "visible in a normal terminal window: $line\n"
            "Please trim to 80 characters or fewer.");
      }
      helpStrings.add(lines[i]);
    }
  }
  commonVerbs.forEach(addVerb);
  if (helpStrings.length > 20) {
    throw new StateError(
        "More than 20 lines in the combined documentation of [commonVerbs]. "
        "The documentation may scroll out of view:\n${helpStrings.join('\n')}."
        "Can you shorten the documentation?");
  }
  if (showAllVerbs) {
    uncommonVerbs.forEach(addVerb);
  }
  return helpStrings.join("\n");
}
