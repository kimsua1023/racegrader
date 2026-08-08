module.exports = {
  extends: ['@commitlint/config-conventional'],
  rules: {
    'type-enum': [
      2,
      'always',
      ['feat', 'fix', 'docs', 'refactor', 'test', 'chore', 'ci'],
    ],
    'scope-enum': [
      2,
      'always',
      ['kernel', 'cli', 'docs', 'ci'],
    ],
    'subject-case': [2, 'always', 'lower-case'],
  },
};
