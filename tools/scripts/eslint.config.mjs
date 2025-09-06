import eslint from '@eslint/js';
import tseslint from 'typescript-eslint';

// https://typescript-eslint.io/getting-started/
export default tseslint.config(
	eslint.configs.recommended,
	// Note:
	//   @typescript-eslint/recommended-type-checked automatically disables the conflicting base rules
	//   that come from eslint:recommended.
	//   See https://typescript-eslint.io/users/configs/#recommended-type-checked
	//   See https://github.com/typescript-eslint/typescript-eslint/blob/main/packages/eslint-plugin/src/configs/flat/recommended-type-checked.ts
	//   See Extension Rules at https://typescript-eslint.io/rules/?=extension#extension-rules
	//   Consider switching to @typescript-eslint/strict-type-checked.
	tseslint.configs.recommendedTypeChecked,
	{
		languageOptions: {
			// https://typescript-eslint.io/packages/parser#configuration
			parserOptions: {
				// https://typescript-eslint.io/packages/parser#ecmaversion
				ecmaVersion: 'latest',
				// https://typescript-eslint.io/packages/parser#projectservice
				projectService: true,
				// https://typescript-eslint.io/packages/parser#tsconfigrootdir
				tsconfigRootDir: import.meta.dirname,
			},
		},

		rules: {
			// https://eslint.org/docs/latest/rules/eqeqeq
			// Require the use of === and !==
			eqeqeq: 'error',

			// https://typescript-eslint.io/rules/no-unused-vars/
			// It is set to 'error' by @typescript-eslint/recommended
			// (which also correctly disables ESLint's base no-unused-vars).
			// Let's decrease to only 'warn'.
			'@typescript-eslint/no-unused-vars': 'warn',

			// https://typescript-eslint.io/rules/strict-boolean-expressions/
			'@typescript-eslint/strict-boolean-expressions': [
				'error',
				{
					allowString: false,
					allowNumber: false,
					allowNullableObject: false,
				},
			],
			// https://typescript-eslint.io/rules/prefer-nullish-coalescing/
			'@typescript-eslint/prefer-nullish-coalescing': 'error',

			// https://typescript-eslint.io/rules/no-misused-promises/
			'@typescript-eslint/no-misused-promises': [
				'error',
				{
					checksVoidReturn: false,
				},
			],

			// // could be useful
			// // https://typescript-eslint.io/rules/class-methods-use-this/
			// 'class-methods-use-this': 'off',
			// '@typescript-eslint/class-methods-use-this': 'error',
		},
	},
	{
		files: ['test/**/*.ts'],
		rules: {
			'@typescript-eslint/no-unused-vars': 'off',
		},
	},
	{
		// global ignores
		// https://eslint.org/docs/latest/use/configure/configuration-files#globally-ignoring-files-with-ignores
		// ignores: [],
	},
);
