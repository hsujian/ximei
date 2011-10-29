#include "mmap_util.h"

int mmap_file_readonly_open(mmap_file_t &mft, const char *filepath)
{
	mft.size = 0;
	mft.mm = NULL;
	struct stat st;
	int fd = open(filepath, O_RDONLY|O_LARGEFILE);
	if (fd < 0) {
		WARNING_LOG("can not open file[%s][%s]", filepath, strerror(errno));
		return -1;
	}
	if(fstat(fd, &st) < -1) {
		WARNING_LOG("fstat file[%s] failed", filepath);
		close(fd);
		return -2;
	}

	mft.size = st.st_size;
	mft.mm = (char *)mmap(NULL, mft.size, PROT_READ, MAP_SHARED, fd, 0);
	if (MAP_FAILED == (void *)mft.mm) {
		FATAL_LOG("can not mmap file[%s][%s]", filepath, strerror(errno));
		close(fd);
		mft.size = 0;
		return -3;
	}
	close(fd);
	return 0;
}

void mmap_file_close(mmap_file_t &mft)
{
	if (mft.size > 0 && (void *) mft.mm != MAP_FAILED) {
		munmap(mft.mm, mft.size);
	}
}

int as_index_load(char *index_path, as_index_t *pindex, int server_id)
{
	snprintf(g_indexpath, sizeof(g_indexpath), "%s", index_path);

	pindex->pterm_db = db_creat(5000000);
	if (pindex->pterm_db == NULL) {
		WARNING_LOG("create term_db failed");
		return -1;
	}

	pindex->pinfo_mem_db = db_load(index_path, "info_db", 5000000);
	if (pindex->pinfo_mem_db == NULL) {
		WARNING_LOG("info_db not load");
		pindex->pinfo_mem_db = db_creat(5000000);
		if (pindex->pinfo_mem_db == NULL) {
			WARNING_LOG("crate info_db failed");
			return -1;
		}
	}
	DEBUG_LOG("load info_db OK");

	pindex->term_line = 0;
	memset(pindex->term_mem, 0, sizeof(pindex->term_mem));
	pindex->term_mem[0].max_num = AS_TERM_MEM_LINE_SIZE;
	pindex->term_mem[0].cur_num = 0;
	pindex->term_mem[0].pindex = (as_term_index_t *)malloc(sizeof(as_term_index_t) * AS_TERM_MEM_LINE_SIZE);
	if (pindex->term_mem[0].pindex == NULL) {
		WARNING_LOG("malloc for as_term_index_t failed!");
		return -1;
	}

	char file_path[MAX_PATH_LEN];
	snprintf(file_path, sizeof(file_path), "%s/term_index_1", index_path);

	mmap_file_t term_index_mft;
	Tydict_node snode;
	if (mmap_file_readonly_open(term_index_mft, file_path) != 0) {
		FATAL_LOG("can not mmap file open file[%s]", file_path);
		return -1;
	} else {
		mmap_file_t info_mft[AS_INFO_MAX_FD_NUM];
		memset(info_mft, 0, sizeof(info_mft));
		{
			int i = 0;
			for (i = 0; i<AS_INFO_MAX_FD_NUM ;i++) {
				snprintf(file_path, sizeof(file_path), "%s/term_index_2_%d", index_path, i);
				if (mmap_file_readonly_open(info_mft[i], file_path) != 0) {
					DEBUG_LOG("no more file[%s]", file_path);
					break;
				}
			}
			if (i == AS_INFO_MAX_FD_NUM) {
				WARNING_LOG("dangerous!! term_index_2_? file number may be over %d", AS_INFO_MAX_FD_NUM);
			}
		}
		DEBUG_LOG("open term_index_2_? OK");

		int term_num = term_index_mft.size / sizeof(as_term_index_t);
		as_term_index_t *term_ptr = (as_term_index_t *) term_index_mft.mm;
		as_term_index_t *term_end = term_ptr + term_num;
		for (;term_ptr < term_end; term_ptr++) {
			// just foreach
			if (term_ptr->term_order >= AS_INFO_MAX_FD_NUM) {
				WARNING_LOG("term order error too big");
				continue;
			}

			if (pindex->term_mem[pindex->term_line].cur_num >= AS_TERM_MEM_LINE_SIZE) {
				if (pindex->term_line >= AS_TERM_MAX_MEM_LINE_NUM -1) {
					WARNING_LOG("term buffer is full");
					break;
				}
				as_term_index_t *pindex_tmp = (as_term_index_t *)malloc(sizeof(as_term_index_t) * AS_TERM_MEM_LINE_SIZE);
				if (pindex_tmp == NULL) {
					FATAL_LOG("malloc for as_term_index_t failed!");
					mmap_file_close(term_index_mft);
					return -1;
				}
				pindex->term_line++;
				pindex->term_mem[pindex->term_line].max_num = AS_TERM_MEM_LINE_SIZE;
				pindex->term_mem[pindex->term_line].cur_num = 0;
				pindex->term_mem[pindex->term_line].pindex = pindex_tmp;
			}

			int term_order = pindex->term_line;
			int term_offset = pindex->term_mem[pindex->term_line].cur_num;

			pindex->term_mem[term_order].pindex[term_offset] = *term_ptr;
			pindex->term_mem[term_order].pindex[term_offset].index_type = 0;
			as_term_node_t *nodes = (as_term_node_t *)malloc(sizeof(as_term_node_t) * AS_INDEX_BLOCK_SIZE * term_ptr->term_block_num);
			pindex->term_mem[term_order].pindex[term_offset].nodes = nodes;
			if (nodes == NULL) {
				FATAL_LOG("malloc for term node failed! leave the nodes in file");
			} else {
				memcpy(nodes, info_mft[term_ptr->term_order].mm + term_ptr->term_index,
						sizeof(as_term_node_t) * AS_INDEX_BLOCK_SIZE * term_ptr->term_block_num);
				pindex->term_mem[term_order].pindex[term_offset].index_type = 1;
			}

			snode.sign1 = term_ptr->sign1;
			snode.sign2 = term_ptr->sign2;
			snode.other = term_order;
			snode.other2 = term_offset;
			snode.code = -1;
			db_op1(pindex->pterm_db, &snode, ADD);
			pindex->term_mem[pindex->term_line].cur_num++;
		}

		mmap_file_close(term_index_mft);
		for (int i = AS_INFO_MAX_FD_NUM; i--;) {
			mmap_file_close(info_mft[i]);
		}
	}
	DEBUG_LOG("load term_index_1 OK");

	//load info
	pindex->info_line = 0;
	memset(pindex->info_mem, 0, sizeof(pindex->info_mem));
	mmap_file_t mft;
	for (pindex->info_line = 0; ;pindex->info_line++) {
		snprintf(file_path, sizeof(file_path), "%s/info_%d_%d.source", index_path, server_id, pindex->info_line);
		int ret = mmap_file_readonly_open(mft, file_path);
		if (ret == -1) {
			//WARNING_LOG("can not open file[%s]", file_path);
			break;
		}
		if(ret != 0) {
			//WARNING_LOG("mmap file open fail [%s]", file_path);
			return -1;
		}
		if (mft.size % sizeof(as_info_node_mem_t) != 0 || mft.size > sizeof(as_info_node_mem_t) * AS_INFO_MEM_LINE_SIZE) {
			FATAL_LOG("file[%s] has been distroyed", file_path);
			mmap_file_close(mft);
			return -1;
		}

		as_info_node_mem_t *ptr = (as_info_node_mem_t *)malloc(sizeof(as_info_node_mem_t) * AS_INFO_MEM_LINE_SIZE);
		if (ptr == NULL) {
			FATAL_LOG("malloc for info line failed!");
			return -1;
		}
		pindex->info_mem[pindex->info_line].max_num = AS_INFO_MEM_LINE_SIZE;
		pindex->info_mem[pindex->info_line].cur_num = mft.size / sizeof(as_info_node_mem_t);
		pindex->info_mem[pindex->info_line].pbuf = ptr;

		memcpy(ptr, mft.mm, mft.size);
		mmap_file_close(mft);
	}
	if (pindex->info_line == 0) {
		FATAL_LOG("no info_%d_?.source", server_id);
		return -1;
	}
	pindex->info_line--;
	DEBUG_LOG("load info_%d_%d.source OK", server_id, pindex->info_line);

	// open info org_file
	char dat_file[MAX_PATH_LEN];
	for (int i = 0; i < AS_INFO_MAX_FD_NUM; i++) {
		snprintf(dat_file, sizeof(dat_file), "%s/info_org_%d_%d.dat", index_path, server_id, i);
		pindex->info_org_fd[i] = open(dat_file, O_RDWR|O_CREAT|O_LARGEFILE, 0644);
		if (pindex->info_org_fd[i] < 0) {
			FATAL_LOG("can't open or create file[%s]", dat_file);
		}
#ifdef _ENABLE_INFO_ORG_READ_FD
		pindex->info_org_read_fd[i] = open(dat_file, O_RDONLY);
#endif
	}
	struct stat st;
	int st_ret = 0;
	pindex->info_org_cur_id = 0;
	do {
		memset(&st, 0 ,sizeof(struct stat));
		st_ret = fstat(pindex->info_org_fd[pindex->info_org_cur_id], &st);
		if( st_ret < 0 || st.st_size > AS_MAX_INFO_FILE_SIZE) {
			if (pindex->info_org_cur_id >= AS_INFO_MAX_FD_NUM) {
				WARNING_LOG("server is full");
				return -1;
			}
			pindex->info_org_cur_id++;
		}
	}while(st_ret < 0 || st.st_size > AS_MAX_INFO_FILE_SIZE);
	DEBUG_LOG("open info_org_%d_*.dat info_org_cur_id=%d/%d OK", server_id, pindex->info_org_cur_id, AS_INFO_MAX_FD_NUM);
	return 0;
}

int as_index_del_from_term_index(as_index_t *pindex, as_parse_word_t *pword, int wp_num, int info_line, int info_index)
{
	int file_type[8];
	int fy_num = 0;
	for (int i = 0; i < wp_num; i++) {
		int j = 0;
		for (; j < fy_num; j++) {
			if (file_type[j] == pword[i].type) {
				break;
			}
		}
		if (j == fy_num && fy_num < 8) {
			file_type[fy_num] = pword[i].type;
			fy_num++;
		}
	}

	Tydict_node snode;
	snode.sign1 = pword[0].sign1;
	snode.sign2 = pword[0].sign2;
	snode.code = -1;
	snode.other = 0; 
	snode.other2 = 0;
	db_op1(pindex->pterm_db, &snode, SEEK);
	if (snode.code < 0) {
		DEBUG_LOG("term[%u][%u][%s] not in term db", pword[0].sign1, pword[0].sign2, pword[0].word);
		return -1;
	}
	
	int term_line = snode.other;
	int id = snode.other2;
	as_term_node_t *ptmp = NULL;
	as_term_node_t *pt = NULL;
	int block_num = pindex->term_mem[term_line].pindex[id].term_block_num;
	as_term_node_t *nodes = (as_term_node_t *)malloc(sizeof(as_term_node_t) * block_num * AS_INDEX_BLOCK_SIZE);
	if (nodes == NULL) {
		WARNING_LOG("malloc for nodes tmp block failed");
		return -1;
	}
	int term_num = pindex->term_mem[term_line].pindex[id].term_num;
	int i = 0;
	int tn = 0;
	for (i = 0; i < term_num; i++) {
		pt = pindex->term_mem[term_line].pindex[id].nodes + i;
		if ((int)pt->info_index != info_index || (int)pt->info_order != info_line) {
			memcpy(nodes + tn, pt, sizeof(as_term_node_t));
			tn++;
			continue;
		}
		int j = 0;
		for (; j < fy_num; j++) {
			if ((u_int)file_type[j] == pt->field_id) {
				break;
			}
		}
		if (j == fy_num) {
			memcpy(nodes + tn, pt, sizeof(as_term_node_t));
			tn++;
			continue;
		}
	}
	pindex->term_mem[term_line].pindex[id].term_num = tn;
	ptmp = pindex->term_mem[term_line].pindex[id].nodes;
	pindex->term_mem[term_line].pindex[id].nodes = nodes;
	if (as_index_reclaim_input(ptmp) == -1) {
		WARNING_LOG("free in as_index_del:%#x", ptmp);
		free(ptmp);
	}
	return 0;
}

int as_index_add_to_term_index(as_index_t *pindex, as_parse_word_t *pword, int wp_num, int info_line, int info_index)
{
	Tydict_node snode;
	snode.sign1 = pword[0].sign1;
	snode.sign2 = pword[0].sign2;
	snode.code = -1;
	snode.other = 0; 
	snode.other2 = 0;
	db_op1(pindex->pterm_db, &snode, SEEK);

	as_term_node_t *nodes = NULL;
	as_term_node_t *ptmp = NULL;
	if (snode.code < 0) {
		// new term
		int block_num = wp_num / AS_INDEX_BLOCK_SIZE + 1;
		if (pindex->term_mem[pindex->term_line].cur_num >= pindex->term_mem[pindex->term_line].max_num) {
			if (pindex->term_line >= AS_TERM_MAX_MEM_LINE_NUM -1) {
				WARNING_LOG("term buffer is full");
				return -1;
			}
			as_term_index_t *ptr = (as_term_index_t *)malloc(sizeof(as_term_index_t) * AS_TERM_MEM_LINE_SIZE);
			if (ptr == NULL) {
				FATAL_LOG("malloc for as_term_index_t failed!");
				return -1;
			}
			pindex->term_line++;
			pindex->term_mem[pindex->term_line].max_num = AS_TERM_MEM_LINE_SIZE;
			pindex->term_mem[pindex->term_line].cur_num = 0;
			pindex->term_mem[pindex->term_line].pindex = ptr;
		}
		nodes = (as_term_node_t *)malloc(sizeof(as_term_node_t) * block_num * AS_INDEX_BLOCK_SIZE);
		if (nodes == NULL) {
			WARNING_LOG("malloc for new term block failed");
			return -1;
		}
		int id = pindex->term_mem[pindex->term_line].cur_num;
		pindex->term_mem[pindex->term_line].cur_num++;
		as_term_index_t *pterm_index = & (pindex->term_mem[pindex->term_line].pindex[id]);

		pterm_index->sign1 = pword[0].sign1;
		pterm_index->sign2 = pword[0].sign2;
		pterm_index->term_num = wp_num;
		pterm_index->term_block_num = block_num;
		pterm_index->doc_num = 1;

		for(int i = wp_num; i--;) {
			nodes[i].field_id = pword[i].type;
			nodes[i].info_index = info_index;
			nodes[i].info_order = info_line;
			nodes[i].offset = pword[i].offset;
			nodes[i].term_num = pword[i].num;
		}

		pterm_index->nodes = nodes;
		snode.code = -1;
		snode.other = pindex->term_line;
		snode.other2 = id;
		db_op1(pindex->pterm_db, &snode, ADD);
	} else {
		int term_line = snode.other;
		int id = snode.other2;
		int i = 0;

		as_term_node_t *p_orig_nodes = pindex->term_mem[term_line].pindex[id].nodes;
		const int orig_term_num = pindex->term_mem[term_line].pindex[id].term_num;
		const int orig_block_num = pindex->term_mem[term_line].pindex[id].term_block_num;

		if (
				/* nodes Can Additional */
				(orig_block_num * AS_INDEX_BLOCK_SIZE >= orig_term_num + wp_num)
				&&
				/* Order & Index Can Additional*/
				(orig_term_num < 1 || (p_orig_nodes[orig_term_num-1].info_order <= info_line && p_orig_nodes[orig_term_num-1].info_index <= info_index))
		   ) {
			i = orig_term_num;
			for(int j = 0; j < wp_num ; j++, i++) {
				p_orig_nodes[i].field_id = pword[j].type;
				p_orig_nodes[i].info_index = info_index;
				p_orig_nodes[i].info_order = info_line;
				p_orig_nodes[i].offset = pword[j].offset;
				p_orig_nodes[i].term_num = pword[j].num;
			}

		} else {
			int block_num = (orig_term_num + wp_num) / AS_INDEX_BLOCK_SIZE + 1;
			nodes = (as_term_node_t *)malloc(sizeof(as_term_node_t) * block_num * AS_INDEX_BLOCK_SIZE);
			if (nodes == NULL) {
				WARNING_LOG("malloc for nodes tmp block failed");
				return -1;
			}
			// optimize here
			{
				as_term_node_t *p_insert = NULL;

				as_term_node_t term_node;
				term_node.info_order = info_line;
				term_node.info_index = info_index;

				p_insert = upper_bound (p_orig_nodes, p_orig_nodes+orig_term_num, term_node, cmp_term_node);

				i = p_insert - p_orig_nodes;
				if (i > 0) {
					memcpy(nodes, p_orig_nodes, sizeof(as_term_node_t) * i);
				}

				for(int j = 0; j < wp_num ; j++, i++) {
					nodes[i].field_id = pword[j].type;
					nodes[i].info_index = info_index;
					nodes[i].info_order = info_line;
					nodes[i].offset = pword[j].offset;
					nodes[i].term_num = pword[j].num;
				}

				if (orig_term_num > (p_insert - p_orig_nodes)) {
					memcpy(nodes + i, p_insert, sizeof(as_term_node_t) * (orig_term_num - (p_insert - p_orig_nodes)));
				}

			}
			//end

			ptmp = pindex->term_mem[term_line].pindex[id].nodes;
			pindex->term_mem[term_line].pindex[id].nodes = nodes;
			pindex->term_mem[term_line].pindex[id].term_block_num = block_num;

			if (as_index_reclaim_input(ptmp) == -1) {
				WARNING_LOG("free in as_index_add:%#x", ptmp);
				free(ptmp);
			}
		}
		pindex->term_mem[term_line].pindex[id].doc_num++;
		pindex->term_mem[term_line].pindex[id].term_num += wp_num;
	}
	return 0;
}

int as_index_add_info_work(as_index_t *pindex, scws_t &s, as_info_t *pinfo, char *content, int info_line, int info_index)
{
#ifdef DEBUG_TIME
	struct timeval tv1, tv2;
	int timeused;
#endif
	int max_word_num = 40960;
	as_parse_word_t *pword = (as_parse_word_t *)malloc(sizeof(as_parse_word_t) * max_word_num);
	if (pword == NULL) {
		WARNING_LOG("malloc for as_parse_word_t failed");
		return -1;
	}

	int wp_num = as_index_parse_info(s, pinfo, content, &(pindex->index_conf), pword, max_word_num);

	int start_id = 0; 
	int end_id = 0;
	while (start_id < wp_num) {
		end_id = start_id + 1;
		while (end_id < wp_num) {
			if (pword[end_id].sign1 == pword[start_id].sign1 && pword[end_id].sign2 == pword[start_id].sign2) {
				end_id++;
			} else {
				break;
			}
		}

#ifdef DEBUG_TIME
		GetTimeCurrent(tv1);
#endif
		as_index_add_to_term_index(pindex, pword+start_id, end_id - start_id, info_line, info_index);
#ifdef DEBUG_TIME
		GetTimeCurrent(tv2);
		SetTimeUsed(timeused, tv1, tv2);
		DEBUG_LOG_IF((timeused>1), "as_index_add_to_term_index %d [%s]", timeused, pword[start_id].word);
#endif
		start_id = end_id;
	}
	free(pword);
	return 0;
}

int as_index_add_info(as_index_t *pindex, scws_t &s, as_info_t *pinfo, char *content)
{
	Tydict_node snode;
	snode.sign1 = pinfo->key1;
	snode.sign2 = pinfo->key2;
	snode.code = -1;
	snode.other = 0;
	snode.other2 = 0;
	db_op1(pindex->pinfo_mem_db, &snode, SEEK);
	if (snode.code >= 0) {
		DEBUG_LOG("DOC_ALREADY_EXIST key1[%u] key2[%u]", pinfo->key1, pinfo->key2);
		return DOC_ALREADY_EXIST;
	}

	//check cur org file 
	struct stat st;
	int st_ret = 0;
	do {
		if (pindex->info_org_cur_id >= AS_INFO_MAX_FD_NUM) {
			WARNING_LOG("server is full");
			return DATA_OTHER_ERROR;
		}
		memset(&st, 0 ,sizeof(struct stat));
		st_ret = fstat(pindex->info_org_fd[pindex->info_org_cur_id], &st);
		if( st_ret < 0 || st.st_size > AS_MAX_INFO_FILE_SIZE) {
			pindex->info_org_cur_id++;
		} else {
			break;
		}
	} while(1);

	// write org file
	u_int offset = 0;//save write offset
	as_writeend(pindex->info_org_fd[pindex->info_org_cur_id], pinfo, sizeof(as_info_t), offset);
	as_writeeof(pindex->info_org_fd[pindex->info_org_cur_id], content, pinfo->content_len);

	// input info mem lines
	if (pindex->info_mem[pindex->info_line].cur_num >= pindex->info_mem[pindex->info_line].max_num) {
		if (pindex->info_line + 1 >= AS_INFO_MAX_MEM_LINE_NUM) {
			WARNING_LOG("the info num is too bug than %d * %d", AS_INFO_MAX_MEM_LINE_NUM, AS_INFO_MEM_LINE_SIZE);
			return DATA_OTHER_ERROR;
		}
		as_info_node_mem_t *pbuf = (as_info_node_mem_t *)malloc(sizeof(as_info_node_mem_t) * AS_INFO_MEM_LINE_SIZE);
		if (pbuf == NULL) {
			WARNING_LOG("malloc for info line failed!");
			return DATA_OTHER_ERROR;
		}
		pindex->info_line++;
		pindex->info_mem[pindex->info_line].max_num = AS_INFO_MEM_LINE_SIZE;
		pindex->info_mem[pindex->info_line].cur_num = 0;
		pindex->info_mem[pindex->info_line].pbuf = pbuf;
	}
	as_info_node_mem_t *pbuf = pindex->info_mem[pindex->info_line].pbuf;
	int k = pindex->info_mem[pindex->info_line].cur_num;
	pindex->info_mem[pindex->info_line].cur_num++;

	pbuf[k].key1 = pinfo->key1;
	pbuf[k].key2 = pinfo->key2;
	for (int i = 0; i < AS_INFO_ATTR_INT_NUM; i++) {
		pbuf[k].attr_int[i] = pinfo->attr_int[i];
	}
	pbuf[k].attr_discrete = pinfo->attr_discrete;
	pbuf[k].is_del = 0;
	pbuf[k].info_file_order = pindex->info_org_cur_id;
	pbuf[k].info_file_offset = offset;

	// add to pinfo db
	snode.sign1 = pinfo->key1;
	snode.sign2 = pinfo->key2;
	snode.code = -1;
	snode.other = pindex->info_line;
	snode.other2 = k;
	db_op1(pindex->pinfo_mem_db, &snode, ADD);

	// parse info
	if(as_index_add_info_work(pindex, s, pinfo, content, pindex->info_line, k) < 0) {
		return DATA_OTHER_ERROR;
	}

	return OK;
}

int as_index_mod_info_int(as_index_t *pindex, as_info_t *pinfo, int update_data_int)
{
	int bmod = 0;
	Tydict_node snode;
	snode.sign1 = pinfo->key1;
	snode.sign2 = pinfo->key2;
	snode.code = -1;
	snode.other = 0;
	snode.other2 = 0;
	db_op1(pindex->pinfo_mem_db, &snode, SEEK);
	if (snode.code < 0) {
		return DOC_NOT_EXIST;
	}
	as_info_node_mem_t *pnode = &(pindex->info_mem[snode.other].pbuf[snode.other2]);
	if (pnode->key1 != pinfo->key1 || pnode->key2 != pinfo->key2) {
		WARNING_LOG("info mem error at[%u][%u]", snode.other, snode.other2);
		return DATA_OTHER_ERROR;
	}

	if (pnode->attr_discrete != pinfo->attr_discrete) {
		pnode->attr_discrete = pinfo->attr_discrete;
		bmod = 1;
	}
	for (int i = 0; i < AS_INFO_ATTR_INT_NUM; i++) {
		if (pnode->attr_int[i] != pinfo->attr_int[i]) {
			pnode->attr_int[i] = pinfo->attr_int[i];
			bmod = 1;
		}
	}

	if (bmod && update_data_int) {
		as_info_t i_node;
		memset(&i_node, 0, sizeof(as_info_t));
		as_readat(pindex->info_org_fd[pnode->info_file_order], &i_node, sizeof(as_info_t), pnode->info_file_offset);

		i_node.attr_discrete = pinfo->attr_discrete;
		for (int i = 0; i < AS_INFO_ATTR_INT_NUM; i++) {
			i_node.attr_int[i] = pinfo->attr_int[i];
		}
		as_writeat(pindex->info_org_fd[pnode->info_file_order], &i_node, sizeof(as_info_t), pnode->info_file_offset);
	}

	return OK;
}

int as_index_del_info(as_index_t *pindex, u_int key1, u_int key2, int hidden)
{
	Tydict_node snode;
	snode.sign1 = key1;
	snode.sign2 = key2;
	snode.code = -1;
	snode.other = 0;
	snode.other2 = 0;
	db_op1(pindex->pinfo_mem_db, &snode, SEEK);
	if (snode.code < 0) {
		return DOC_NOT_EXIST;
	}

	as_info_node_mem_t *pnode = &(pindex->info_mem[snode.other].pbuf[snode.other2]);
	if (pnode->key1 != key1 || pnode->key2 != key2) {
		WARNING_LOG("info mem error at[%u][%u]", snode.other, snode.other2);
		return DATA_OTHER_ERROR;
	}

	if (hidden) {
		if (pnode->is_del == AS_INFO_STATUS_HIDDEN) {
			return OK;
		}
		pnode->is_del = AS_INFO_STATUS_HIDDEN;
	} else {
		if (pnode->is_del == AS_INFO_OK) {
			return OK;
		}
		pnode->is_del = AS_INFO_OK;
	}

	as_info_t i_node;
	memset(&i_node, 0, sizeof(as_info_t));
	as_readat(pindex->info_org_fd[pnode->info_file_order], &i_node, sizeof(as_info_t), pnode->info_file_offset);

	i_node.is_del = pnode->is_del;

	as_writeat(pindex->info_org_fd[pnode->info_file_order], &i_node, sizeof(as_info_t), pnode->info_file_offset);
	
	return OK;
}

int as_index_mod_info(as_index_t *pindex, scws_t &s, as_info_t *pinfo, char *content)
{
	Tydict_node snode;
	snode.sign1 = pinfo->key1;
	snode.sign2 = pinfo->key2;
	snode.code = -1;
	snode.other = 0;
	snode.other2 = 0;
	db_op1(pindex->pinfo_mem_db, &snode, SEEK);
	if (snode.code < 0) {
		return DOC_NOT_EXIST;
	}

	as_info_node_mem_t *pnode = &(pindex->info_mem[snode.other].pbuf[snode.other2]);
	if (pnode->key1 != pinfo->key1 || pnode->key2 != pinfo->key2) {
		WARNING_LOG("info mem error at[%u][%u]", snode.other, snode.other2);
		return DATA_OTHER_ERROR;
	}
	// ÏÈÈÃinfoÍ£Ö¹Ìá¹©ËÑË÷
	pnode->is_del = AS_INFO_STATUS_HIDDEN;

	as_info_t i_node;
	memset(&i_node, 0, sizeof(as_info_t));
	as_readat(pindex->info_org_fd[pnode->info_file_order], &i_node, sizeof(as_info_t), pnode->info_file_offset);

	char content_tmp[AS_MAX_CONTENT_LEN];
	memset(content_tmp, 0, sizeof(content_tmp));
	if (i_node.content_len >= AS_MAX_CONTENT_LEN) {
		as_readeof(pindex->info_org_fd[pnode->info_file_order], content_tmp, AS_MAX_CONTENT_LEN -1);
	} else {
		as_readeof(pindex->info_org_fd[pnode->info_file_order], content_tmp, i_node.content_len);
	}

	// save org file
	if (i_node.content_len == pinfo->content_len) {
		as_writeat(pindex->info_org_fd[pnode->info_file_order], pinfo, sizeof(as_info_t), pnode->info_file_offset);
		as_writeeof(pindex->info_org_fd[pnode->info_file_order], content, pinfo->content_len);
	} else {
		// delete the old info struct
		i_node.is_del = AS_INFO_STATUS_DELETE;
		as_writeat(pindex->info_org_fd[pnode->info_file_order], &i_node, sizeof(as_info_t), pnode->info_file_offset);
		struct stat st;
		int st_ret = 0;
		do {
			if (pindex->info_org_cur_id >= AS_INFO_MAX_FD_NUM) {
				WARNING_LOG("server is full");
				return DATA_OTHER_ERROR;
			}
			memset(&st, 0 ,sizeof(struct stat));
			st_ret = fstat(pindex->info_org_fd[pindex->info_org_cur_id], &st);
			if( st_ret < 0 || st.st_size > AS_MAX_INFO_FILE_SIZE) {
				pindex->info_org_cur_id++;
			} else {
				break;
			}
		} while(1);
		
		u_int offset = 0;
		as_writeend(pindex->info_org_fd[pindex->info_org_cur_id], pinfo, sizeof(as_info_t), offset);
		as_writeeof(pindex->info_org_fd[pindex->info_org_cur_id], content, pinfo->content_len);	
		pnode->info_file_order = pindex->info_org_cur_id;
		pnode->info_file_offset = offset;
#ifdef USE_INFO_CACHE
		as_info_cache_set_unvalid(pindex->info_cache, pnode->info_file_order, pnode->info_file_offset);
#endif
	}


	// parse info
	int max_word_num = 40960;
	as_parse_word_t *pword_old = (as_parse_word_t *)malloc(sizeof(as_parse_word_t) * max_word_num);
	if (pword_old == NULL) {
		WARNING_LOG("malloc for as_parse_word_t failed");
		return DATA_OTHER_ERROR;
	}

	as_parse_word_t *pword_new = (as_parse_word_t *)malloc(sizeof(as_parse_word_t) * max_word_num);
	if (pword_new == NULL) {
		WARNING_LOG("malloc for as_parse_word_t failed");
		free(pword_old);
		return DATA_OTHER_ERROR;
	}
	
	int n1 = 0;
	int left_old = max_word_num;
	int left_new = max_word_num;
	int wp_num_old = 0;
	int wp_num_new = 0;

	if (strcasecmp(i_node.short_str[0], pinfo->short_str[0]) != 0) {
		n1 = as_index_parse_word(s, &(pindex->index_conf.short_str[0]), i_node.short_str[0], AS_TERM_IN_SHORT_STR_0, 
								 pword_old+wp_num_old, left_old);
		wp_num_old += n1;
		left_old -= n1;

		n1 = as_index_parse_word(s, &(pindex->index_conf.short_str[0]), pinfo->short_str[0], AS_TERM_IN_SHORT_STR_0, 
								 pword_new+wp_num_new, left_new);
		wp_num_new += n1;
		left_new -= n1;
	}

	if (strcasecmp(i_node.short_str[1], pinfo->short_str[1]) != 0) {
		n1 = as_index_parse_word(s, &(pindex->index_conf.short_str[1]), i_node.short_str[1], AS_TERM_IN_SHORT_STR_1, 
								 pword_old+wp_num_old, left_old);
		wp_num_old += n1;
		left_old -= n1;

		n1 = as_index_parse_word(s, &(pindex->index_conf.short_str[1]), pinfo->short_str[1], AS_TERM_IN_SHORT_STR_1, 
								 pword_new+wp_num_new, left_new);
		wp_num_new += n1;
		left_new -= n1;
	}

	if (strcasecmp(i_node.long_str[0], pinfo->long_str[0]) != 0) {
		n1 = as_index_parse_word(s, &(pindex->index_conf.long_str[0]), i_node.long_str[0], AS_TERM_IN_LONG_STR_0, 
								 pword_old+wp_num_old, left_old);
		wp_num_old += n1;
		left_old -= n1;

		n1 = as_index_parse_word(s, &(pindex->index_conf.long_str[0]), pinfo->long_str[0], AS_TERM_IN_LONG_STR_0, 
								 pword_new+wp_num_new, left_new);
		wp_num_new += n1;
		left_new -= n1;
	}

	if (strcasecmp(i_node.long_str[1], pinfo->long_str[1]) != 0) {
		n1 = as_index_parse_word(s, &(pindex->index_conf.long_str[1]), i_node.long_str[1], AS_TERM_IN_LONG_STR_1, 
								 pword_old+wp_num_old, left_old);
		wp_num_old += n1;
		left_old -= n1;

		n1 = as_index_parse_word(s, &(pindex->index_conf.long_str[1]), pinfo->long_str[1], AS_TERM_IN_LONG_STR_1, 
								 pword_new+wp_num_new, left_new);
		wp_num_new += n1;
		left_new -= n1;
	}

	if (strcasecmp(content_tmp, content) != 0) {
		n1 = as_index_parse_word(s, &(pindex->index_conf.content), content_tmp, AS_TERM_IN_CONTENT, 
								 pword_old+wp_num_old, left_old);
		wp_num_old += n1;
		left_old -= n1;

		n1 = as_index_parse_word(s, &(pindex->index_conf.content), content, AS_TERM_IN_CONTENT, 
								 pword_new+wp_num_new, left_new);
		wp_num_new += n1;
		left_new -= n1;
	}

	sort(pword_old, pword_old+wp_num_old, cmp_term_sign);
	sort(pword_new, pword_new+wp_num_new, cmp_term_sign);

	int start_id = 0; 
	int end_id = 0;
	while (start_id < wp_num_old) {
		end_id = start_id + 1;
		while (end_id < wp_num_old) {
			if (pword_old[end_id].sign1 == pword_old[start_id].sign1 && pword_old[end_id].sign2 == pword_old[start_id].sign2) {
				end_id++;
			} else {
				break;
			}
		}
		as_index_del_from_term_index(pindex, pword_old+start_id, end_id - start_id, snode.other, snode.other2);
		start_id = end_id;
	}

	start_id = 0;
	end_id = 0;
	while (start_id < wp_num_new) {
		end_id = start_id + 1;
		while (end_id < wp_num_new) {
			if (pword_new[end_id].sign1 == pword_new[start_id].sign1 && pword_new[end_id].sign2 == pword_new[start_id].sign2) {
				end_id++;
			} else {
				break;
			}
		}
		as_index_add_to_term_index(pindex, pword_new+start_id, end_id - start_id, snode.other, snode.other2);
		start_id = end_id;
	}

	free(pword_old);
	free(pword_new);

	pnode->is_del = AS_INFO_OK;
	return OK;
}

int as_index_save(as_index_t *pindex, int server_id, int flags)
{
	char file_path[PATH_SIZE];
	int fd = 0;
	struct stat st;
	snprintf(file_path, sizeof(file_path), "%s/term_index_1", g_indexpath);
	int fd_term = open(file_path,  O_WRONLY|O_CREAT|O_TRUNC|O_LARGEFILE, 0644);
	if (fd_term < 0) {
		WARNING_LOG("can not open file[%s]", file_path);
		return -1;
	}
	snprintf(file_path, sizeof(file_path), "%s/term_index_2_0", g_indexpath);
	fd = open(file_path, O_WRONLY|O_CREAT|O_TRUNC|O_LARGEFILE, 0644);
	if (fd < 0) {
		WARNING_LOG("can not open file[%s]", file_path);
		close(fd_term);
		return -1;
	}

	u_int offset = 0;
	int   file_id = 0;
	as_term_index_t *ptmp;
	as_term_index_t term_index;
	for (int i = 0; i <= pindex->term_line; i++) {
		DEBUG_LOG("term_line[%d]cur_num[%d]", i, pindex->term_mem[i].cur_num);
		for (int j = 0; j < pindex->term_mem[i].cur_num; j++) {
			if((fstat(fd, &st) != -1) && (st.st_size > AS_MAX_INDEX_FILE_SIZE)) {
				close(fd);
				file_id++;
				snprintf(file_path, sizeof(file_path), "%s/term_index_2_%d", g_indexpath, file_id);
				fd = open(file_path, O_WRONLY|O_CREAT|O_TRUNC|O_LARGEFILE, 0644);
				if (fd < 0) {
					WARNING_LOG("can not open file[%s]", file_path);
					close(fd_term);
					return -1;
				}
			}
			ptmp = &(pindex->term_mem[i].pindex[j]);
			offset = 0;
			as_writeend(fd, ptmp->nodes, sizeof(as_term_node_t) * AS_INDEX_BLOCK_SIZE * ptmp->term_block_num, offset);
			
			memset(&term_index, 0, sizeof(term_index));
			term_index.sign1 = ptmp->sign1;
			term_index.sign2 = ptmp->sign2;
			term_index.term_block_num = ptmp->term_block_num;
			term_index.doc_num = ptmp->doc_num;
			term_index.term_order = file_id;
			term_index.term_index = offset;
			term_index.term_num = ptmp->term_num;

			offset = 0;
			as_writeend(fd_term, &term_index, sizeof(term_index), offset);
			
		}
	}
	close(fd);
	close(fd_term);

	char info_file[MAX_PATH_LEN];
	for (int i = 0 ; i <= pindex->info_line; i++) {
		snprintf(info_file, sizeof(info_file), "%s/info_%d_%d.source", g_indexpath, server_id, i);
		int fdw = open(info_file, O_WRONLY|O_CREAT|O_TRUNC|O_LARGEFILE, 0644);
		if (fdw < 0) {
			WARNING_LOG("can not open file[%s]", info_file);
			return -1;
		}
		as_writeeof(fdw, pindex->info_mem[i].pbuf, sizeof(as_info_node_mem_t) * pindex->info_mem[i].cur_num);
		close(fdw);
	}

	//save pinfo_mem_db
	db_save(pindex->pinfo_mem_db, g_indexpath, "info_db");

	//save pterm_db
	db_save(pindex->pterm_db, g_indexpath, "term_db");

	return 0;	
}

