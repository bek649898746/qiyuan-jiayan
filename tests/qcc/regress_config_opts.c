#include <stdio.h>
#include <stddef.h>

typedef int (*config_parser_event_fn_t)(int type, size_t begin, size_t end,
					void *cs, void *data);

struct config_options {
	unsigned int respect_includes : 1;
	unsigned int ignore_repo : 1;
	unsigned int ignore_worktree : 1;
	unsigned int ignore_cmdline : 1;
	unsigned int system_gently : 1;
	unsigned int unconditional_remote_url : 1;
	const char *commondir;
	const char *git_dir;
	config_parser_event_fn_t event_fn;
	void *event_fn_data;
};

int main(void)
{
	printf("sizeof=%d\n", (int)sizeof(struct config_options));
	printf("event_fn_off=%d\n", (int)offsetof(struct config_options, event_fn));
	printf("event_fn_data_off=%d\n", (int)offsetof(struct config_options, event_fn_data));
	return 0;
}
